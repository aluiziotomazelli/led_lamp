/**
 * @file touch.c
 * @brief Touch sensor driver implementation with gesture detection
 * 
 * @details This file implements a capacitive touch sensor driver with
 * automatic recalibration, debouncing, and support for press/hold gestures.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <stdint.h>

// ESP-IDF drivers
#include "driver/touch_pad.h"
#include "driver/touch_sensor.h"
#include "driver/touch_sensor_common.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// ESP-IDF system services
#define LOG_LOCAL_LEVEL ESP_LOG_INFO  // ✅ Must come before esp_log.h
#include "esp_log.h"
#include "esp_timer.h"

// Project specific headers
#include "touch.h"
#include "project_config.h"

static const char *TAG = "Touch";

/**
 * @brief Internal state machine states for touch button operation
 */
typedef enum {
    TOUCH_WAIT_FOR_PRESS,           ///< Waiting for initial press detection
    TOUCH_DEBOUNCE_PRESS,           ///< Debouncing press event
    TOUCH_WAIT_FOR_RELEASE_OR_HOLD, ///< Monitoring for release or hold
    TOUCH_DEBOUNCE_RELEASE          ///< Debouncing release event
} touch_state_t;

/**
 * @brief Complete touch button instance structure
 */
struct touch_s {
    touch_pad_t pad;                ///< GPIO pad number
    touch_state_t state;            ///< Current state machine state
    uint32_t press_start_time_ms;   ///< Timestamp when press was first detected
    uint32_t last_time_ms;          ///< Last event timestamp

    // Configuration parameters
    uint16_t threshold_percent;     ///< Activation threshold percentage
    uint16_t debounce_press_ms;     ///< Press debounce time in ms
    uint16_t debounce_release_ms;   ///< Release debounce time in ms
    uint16_t hold_time_ms;          ///< Time to trigger hold event

    uint16_t baseline;              ///< Current baseline value

    // Hold event control
    bool hold_generated;            ///< Flag indicating hold event was generated
    bool enable_hold_repeat;        ///< Enable repeated hold events
    uint16_t hold_repeat_interval_ms; ///< Interval between hold events
    uint32_t last_hold_event_ms;    ///< Timestamp of last hold event

    // Recalibration control
    uint64_t recalibration_interval_us; ///< Recalibration interval
    bool is_recalibrating;          ///< Recalibration in progress flag
    bool is_reading;                ///< Touch reading in progress flag
    esp_timer_handle_t recalibration_timer;
    bool timer_initialized;         ///< Timer initialization status

    QueueHandle_t output_queue;     ///< Event output queue
    TaskHandle_t task_handle;       ///< Associated task handle
};

/**
 * @brief Get current time in milliseconds
 * @return Current time in ms
 */
static uint32_t get_current_time_ms() { 
    return esp_timer_get_time() / 1000; 
}

/**
 * @brief ISR handler for touch events
 * 
 * @param arg Touch handle passed as argument
 * 
 * @note This ISR runs in IRAM for fast execution
 * @warning Keep ISR processing time minimal
 */
static void IRAM_ATTR touch_isr_handler(void *arg) {
    touch_t *touch_handle = (touch_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Notify the touch task
    xTaskNotifyFromISR(touch_handle->task_handle, 0, eNoAction,
                      &xHigherPriorityTaskWoken);
    ESP_DRAM_LOGV(TAG, "ISR triggered for pad %d", touch_handle->pad);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Recalibrate touch sensor baseline and threshold
 * 
 * @param touch_handle Touch button instance
 * 
 * @note This function disables interrupts during calibration
 * @warning Calibration should not be performed during active touch events
 */
static void touch_recalibrate(touch_t *touch_handle) {
    ESP_LOGD(TAG, "Recalibration START for pad %d (Baseline: %d)",
            touch_handle->pad, touch_handle->baseline);

    touch_handle->is_recalibrating = true;
    touch_pad_intr_disable();  // Disable interrupts during calibration

    // Take multiple samples for stable baseline
    uint32_t sum = 0;
    const uint8_t samples = 5;
    for (int i = 0; i < samples; i++) {
        uint16_t sample;
        touch_pad_read_raw_data(touch_handle->pad, &sample);
        sum += sample;
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Calculate new baseline and threshold
    touch_handle->baseline = sum / samples;
    uint16_t threshold =
        touch_handle->baseline -
        (touch_handle->baseline * touch_handle->threshold_percent / 100);

    touch_pad_set_thresh(touch_handle->pad, threshold);
    touch_handle->is_recalibrating = false;
    touch_pad_intr_enable();  // Re-enable interrupts

    ESP_LOGD(TAG, "Recalibration COMPLETE for pad %d (New baseline: %d, Threshold: %d)",
            touch_handle->pad, touch_handle->baseline, threshold);
}

/**
 * @brief Timer callback for periodic recalibration
 * 
 * @param arg Touch handle passed as argument
 * 
 * @note This callback runs in timer context, not ISR context
 */
static void recalibration_timer_callback(void *arg) {
    touch_t *touch_handle = (touch_t *)arg;
    if (!touch_handle->is_reading) {
        ESP_LOGV(TAG, "Recalibration timer triggered for pad %d",
                touch_handle->pad);
        touch_recalibrate(touch_handle);
    }
}

/**
 * @brief Main touch state machine logic
 * 
 * @param touch_handle Touch button instance
 * @return Detected event type
 * 
 * @note This function implements a 4-state finite state machine
 *       for reliable touch event detection with debouncing
 */
static touch_event_type_t touch_get_event(touch_t *touch_handle) {
    uint32_t now = get_current_time_ms();
    uint16_t baseline, threshold, touch_value;

    // Read current touch value and check press state
    baseline = touch_handle->baseline;
    threshold = baseline - (baseline * touch_handle->threshold_percent / 100);
    touch_pad_read_filtered(touch_handle->pad, &touch_value);
    bool is_pressed = (baseline - touch_value) > threshold;

    // State machine implementation
    switch (touch_handle->state) {
    case TOUCH_WAIT_FOR_PRESS:
        if (is_pressed) {
            touch_handle->press_start_time_ms = now;
            touch_handle->state = TOUCH_DEBOUNCE_PRESS;
            ESP_LOGD("FSM", "DEBOUNCE_PRESS (Pad: %" PRIu32 ")",
                    (uint32_t)touch_handle->pad);
        }
        break;

    case TOUCH_DEBOUNCE_PRESS:
        if (now - touch_handle->press_start_time_ms > touch_handle->debounce_press_ms) {
            if (is_pressed) {
                touch_handle->state = TOUCH_WAIT_FOR_RELEASE_OR_HOLD;
                ESP_LOGD("FSM", "WAIT_FOR_RELEASE_OR_HOLD (Pad: %" PRIu32 ")",
                        (uint32_t)touch_handle->pad);
            } else {
                touch_handle->state = TOUCH_WAIT_FOR_PRESS;
                ESP_LOGD("FSM", "Premature release, back to WAIT_FOR_PRESS (Pad: %" PRIu32 ")",
                        (uint32_t)touch_handle->pad);
            }
        }
        break;

    case TOUCH_WAIT_FOR_RELEASE_OR_HOLD:
        if (!is_pressed) {
            uint32_t duration = now - touch_handle->press_start_time_ms;
            if (duration < touch_handle->hold_time_ms) {
                // Short press detected
                touch_handle->last_time_ms = now;
                touch_handle->state = TOUCH_DEBOUNCE_RELEASE;
                touch_handle->hold_generated = false;
                return TOUCH_PRESS;
            } else {
                // Long press already handled
                touch_handle->state = TOUCH_WAIT_FOR_PRESS;
                touch_handle->hold_generated = false;
                touch_handle->is_reading = false;
            }
        } else if (now - touch_handle->press_start_time_ms > touch_handle->hold_time_ms) {
            if (!touch_handle->hold_generated) {
                // First hold event
                touch_handle->hold_generated = true;
                touch_handle->last_hold_event_ms = now;
                return TOUCH_HOLD;
            } else if (touch_handle->enable_hold_repeat &&
                      (now - touch_handle->last_hold_event_ms >= touch_handle->hold_repeat_interval_ms)) {
                // Repeated hold event
                touch_handle->last_hold_event_ms = now;
                return TOUCH_HOLD;
            }
        }
        break;

    case TOUCH_DEBOUNCE_RELEASE:
        if (now - touch_handle->last_time_ms > touch_handle->debounce_release_ms) {
            touch_handle->state = TOUCH_WAIT_FOR_PRESS;
            touch_handle->hold_generated = false;
        }
        break;
    }
    return TOUCH_NONE;
}

/**
 * @brief Main touch processing task
 * 
 * @param param Touch handle passed as argument
 * 
 * @note This task handles touch event processing and debouncing
 * @warning Task should not be blocked for long periods
 */
static void touch_task(void *param) {
    touch_t *touch_handle = (touch_t *)param;
    bool processing = true;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!processing && !touch_handle->is_recalibrating) {
            processing = true;
            touch_pad_intr_disable();
            ESP_LOGD(TAG, "Processing touch event for pad %d", touch_handle->pad);
            touch_handle->is_reading = true;
        }

        do {
            touch_event_type_t event_type = touch_get_event(touch_handle);

            if (event_type != TOUCH_NONE) {
                touch_event_t local_event = {
                    .type = event_type,
                    .pad = touch_handle->pad
                };

                if (xQueueSend(touch_handle->output_queue, &local_event,
                              pdMS_TO_TICKS(10)) == pdPASS) {
                    ESP_LOGD(TAG, "Touch pad %d: click %d sent to queue",
                            local_event.pad, local_event.type);
                } else {
                    ESP_LOGW(TAG, "Touch pad %d: FAILED to send click %d to queue",
                            local_event.pad, local_event.type);
                }
                
                processing = false;
                touch_pad_intr_enable();
                if (touch_handle->state != TOUCH_WAIT_FOR_RELEASE_OR_HOLD ||
                    !touch_pad_get_status()) {
                    touch_handle->is_reading = false;
                }
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        } while (processing);
    }
}

/**
 * @brief Create a new touch button instance
 * 
 * @param[in] config Configuration parameters
 * @param[in] output_queue Queue for event output
 * @return touch_t* Handle to new touch instance, NULL on failure
 * 
 * @note The output queue must be created before calling this function
 * @warning Touch sensors require proper PCB layout and calibration
 */
touch_t *touch_create(const touch_config_t *config, QueueHandle_t output_queue) {
    if (!config || !output_queue) {
        ESP_LOGE(TAG, "Invalid arguments: config or output_queue is NULL");
        return NULL;
    }

    touch_t *touch_handle = calloc(1, sizeof(touch_t));
    if (!touch_handle) {
        ESP_LOGE(TAG, "Failed to allocate memory for touch struct");
        return NULL;
    }

    // Initialize configuration
    touch_handle->pad = config->pad;
    touch_handle->output_queue = output_queue;
    touch_handle->threshold_percent = config->threshold_percent;
    touch_handle->debounce_press_ms = config->debounce_press_ms;
    touch_handle->debounce_release_ms = config->debounce_release_ms;
    touch_handle->hold_time_ms = config->hold_time_ms;
    touch_handle->hold_repeat_interval_ms = config->hold_repeat_interval_ms;
    touch_handle->enable_hold_repeat = config->enable_hold_repeat;
    touch_handle->state = TOUCH_WAIT_FOR_PRESS;

    // Hardware initialization
    ESP_ERROR_CHECK(touch_pad_init());
    ESP_ERROR_CHECK(touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V));
    ESP_ERROR_CHECK(touch_pad_config(touch_handle->pad, 0)); // Initial threshold 0

    // Measurement configuration
    ESP_ERROR_CHECK(touch_pad_set_measurement_clock_cycles(0xffff));
    ESP_ERROR_CHECK(touch_pad_set_measurement_interval(0xffff));
    ESP_ERROR_CHECK(touch_pad_sw_start());
    ESP_ERROR_CHECK(touch_pad_filter_start(10));

    // Get initial baseline
    vTaskDelay(pdMS_TO_TICKS(500)); // Stabilization delay
    touch_pad_read_raw_data(touch_handle->pad, &touch_handle->baseline);
    ESP_LOGI(TAG, "Touch pad %" PRIu32 " baseline: %" PRIu32,
            (uint32_t)touch_handle->pad, (uint32_t)touch_handle->baseline);

    // Set initial threshold
    uint16_t threshold = touch_handle->baseline - 
                        (touch_handle->baseline * touch_handle->threshold_percent / 100);
    ESP_ERROR_CHECK(touch_pad_set_thresh(touch_handle->pad, threshold));
    ESP_LOGI(TAG, "Touch pad %" PRIu32 " threshold set to %" PRIu32,
            (uint32_t)touch_handle->pad, (uint32_t)threshold);

    // Interrupt configuration
    ESP_ERROR_CHECK(touch_pad_isr_register(touch_isr_handler, touch_handle));
    ESP_ERROR_CHECK(touch_pad_intr_enable());

    // Configure recalibration timer
    uint32_t interval_min = (config->recalibration_interval_min > 0) ? 
                           config->recalibration_interval_min : 10; // Default 10 minutes
    touch_handle->recalibration_interval_us = interval_min * 60 * 1000000ULL;
    
    ESP_LOGI(TAG, "Recalibration configured for %" PRIu32 " minutes (%" PRIu64 "μs)",
            interval_min, touch_handle->recalibration_interval_us);
    
    const esp_timer_create_args_t timer_args = {
        .callback = &recalibration_timer_callback,
        .arg = touch_handle,
        .name = "touch_recalibrate"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &touch_handle->recalibration_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(touch_handle->recalibration_timer,
                                           touch_handle->recalibration_interval_us));
    touch_handle->timer_initialized = true;

    // Create processing task
    BaseType_t res = xTaskCreate(touch_task, "touch_task", TOUCH_TASK_STACK_SIZE,
                               touch_handle, TOUCH_TASK_PRIORITY, &touch_handle->task_handle);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create touch task for pad %" PRIu32,
                (uint32_t)touch_handle->pad);
        touch_delete(touch_handle);
        return NULL;
    }

    ESP_LOGI(TAG, "Touch button created on pad %" PRIu32, (uint32_t)touch_handle->pad);
    return touch_handle;
}

/**
 * @brief Delete a touch button instance
 * 
 * @param[in] touch_handle Handle to touch instance
 * 
 * @note This function does not delete the event queue
 * @warning Ensure no tasks are using the touch instance before deletion
 */
void touch_delete(touch_t *touch_handle) {
    if (touch_handle) {
        if (touch_handle->timer_initialized) {
            esp_timer_stop(touch_handle->recalibration_timer);
            esp_timer_delete(touch_handle->recalibration_timer);
        }
        if (touch_handle->task_handle) {
            vTaskDelete(touch_handle->task_handle);
        }
        touch_pad_intr_disable();
        touch_pad_isr_deregister(touch_isr_handler, touch_handle);
        free(touch_handle);
        ESP_LOGI(TAG, "Touch button on pad %" PRIu32 " deleted",
                (uint32_t)touch_handle->pad);
    }
}
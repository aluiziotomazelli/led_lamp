/**
 * @file encoder.c
 * @brief Rotary encoder driver implementation with quadrature decoding
 * 
 * @details This file implements a state machine-based rotary encoder decoder
 * with support for both full-step and half-step modes, including acceleration.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ESP-IDF drivers
#include "driver/gpio.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Set log level for this module, must come before esp_log.h
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
// ESP-IDF system services
#include "esp_log.h"
#include "esp_timer.h"

// Project specific headers
#include "encoder.h"
#include "project_config.h"


static const char *TAG = "Encoder";

/**
 * @brief Rotary encoder state machine definitions
 * Based on standard quadrature decoding implementations
 */
#define R_START     0x0  ///< Initial/Resting state
#define DIR_CW      0x10 ///< Clockwise rotation detected
#define DIR_CCW     0x20 ///< Counter-clockwise rotation detected

// Full-step state definitions
#define FS_R_CW_FINAL   0x1
#define FS_R_CW_BEGIN   0x2
#define FS_R_CW_NEXT    0x3
#define FS_R_CCW_BEGIN  0x4
#define FS_R_CCW_FINAL  0x5
#define FS_R_CCW_NEXT   0x6

// Half-step state definitions
#define H_CCW_BEGIN     0x1
#define H_CW_BEGIN      0x2
#define H_START_M       0x3
#define H_CW_BEGIN_M    0x4
#define H_CCW_BEGIN_M   0x5

/**
 * @brief Full-step state transition table
 * 
 * Table format: [current_state][pin_combination] = next_state
 * Pin combination: 00, 01, 10, 11 (A and B bits)
 */
static const unsigned char ttable_full_step[7][4] = {
    /* R_START */     {R_START,    FS_R_CW_BEGIN, FS_R_CCW_BEGIN, R_START},
    /* FS_R_CW_FINAL */ {FS_R_CW_NEXT, R_START,    FS_R_CW_FINAL, R_START | DIR_CW},
    /* FS_R_CW_BEGIN */ {FS_R_CW_NEXT, FS_R_CW_BEGIN, R_START,    R_START},
    /* FS_R_CW_NEXT */  {FS_R_CW_NEXT, FS_R_CW_BEGIN, FS_R_CW_FINAL, R_START},
    /* FS_R_CCW_BEGIN */{FS_R_CCW_NEXT, R_START,    FS_R_CCW_BEGIN, R_START},
    /* FS_R_CCW_FINAL */{FS_R_CCW_NEXT, FS_R_CCW_FINAL, R_START,    R_START | DIR_CCW},
    /* FS_R_CCW_NEXT */ {FS_R_CCW_NEXT, FS_R_CCW_FINAL, FS_R_CCW_BEGIN, R_START},
};

/**
 * @brief Half-step state transition table
 * 
 * Table format: [current_state][pin_combination] = next_state
 * Pin combination: 00, 01, 10, 11 (A and B bits)
 */
static const unsigned char ttable_half_step[6][4] = {
    /* R_START */      {H_START_M, H_CW_BEGIN,  H_CCW_BEGIN, R_START},
    /* H_CCW_BEGIN */  {H_START_M | DIR_CCW, R_START, H_CCW_BEGIN, R_START},
    /* H_CW_BEGIN */   {H_START_M | DIR_CW, H_CW_BEGIN, R_START, R_START},
    /* H_START_M */    {H_START_M, H_CCW_BEGIN_M, H_CW_BEGIN_M, R_START},
    /* H_CW_BEGIN_M */ {H_START_M, H_START_M, H_CW_BEGIN_M, R_START | DIR_CW},
    /* H_CCW_BEGIN_M */{H_START_M, H_CCW_BEGIN_M, H_START_M, R_START | DIR_CCW}
};

/**
 * @brief Encoder instance structure
 */
struct encoder_s {
    gpio_num_t pin_a;                ///< GPIO pin for channel A
    gpio_num_t pin_b;                ///< GPIO pin for channel B
    QueueHandle_t output_queue;      ///< Event output queue
    TaskHandle_t task_handle;        ///< Processing task handle
    volatile uint8_t rotary_state;   ///< Current FSM state
    bool half_step_mode;             ///< Operation mode flag
    bool acceleration_enabled;       ///< Acceleration enable flag
    uint16_t accel_gap_ms;           ///< Acceleration time threshold
    uint8_t accel_max_multiplier;    ///< Maximum step multiplier
    volatile uint32_t last_step_time_ms; ///< Timestamp of last step
};

/**
 * @brief Map a value from one range to another
 * 
 * @param x Input value
 * @param in_min Input range minimum
 * @param in_max Input range maximum
 * @param out_min Output range minimum
 * @param out_max Output range maximum
 * @return Mapped value
 * 
 * @note Handles division by zero by returning out_min if ranges are equal
 */
static inline long map_value(long x, long in_min, long in_max, 
                           long out_min, long out_max) {
    if (in_min == in_max) {
        return out_min;
    }
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/**
 * @brief ISR handler for encoder pin changes
 * 
 * @param arg Encoder instance handle
 * 
 * @note This ISR runs in IRAM for fast execution
 * @warning Keep ISR processing time minimal
 */
static void IRAM_ATTR encoder_isr_handler(void *arg) {
    encoder_handle_t enc = (encoder_handle_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(enc->task_handle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Main encoder processing task
 * 
 * @param arg Encoder instance handle
 * 
 * @note This task handles the actual quadrature decoding and acceleration
 * @warning This task should not be blocked for long periods
 */
static void encoder_task(void *arg) {
    encoder_handle_t enc = (encoder_handle_t)arg;
    uint8_t current_pin_states = 0;

    while (1) {
        // Wait for notification from ISR indicating pin change
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            // Read current state of encoder pins (A and B)
            current_pin_states = (gpio_get_level(enc->pin_a) << 1) | 
                               gpio_get_level(enc->pin_b);

            // Select appropriate state transition table based on mode
            const unsigned char(*current_ttable)[4] =
                enc->half_step_mode ? ttable_half_step : ttable_full_step;

            // Update state machine using current state and pin inputs
            // Lower nibble (0x0F) holds the state index
            enc->rotary_state =
                current_ttable[enc->rotary_state & 0x0F][current_pin_states];

            // Upper nibble (0x30) holds direction flags (DIR_CW or DIR_CCW)
            uint8_t direction = enc->rotary_state & 0x30;
            int steps = 0;

            if (direction == DIR_CW) {
                steps = 1; // Clockwise step
            } else if (direction == DIR_CCW) {
                steps = -1; // Counter-clockwise step
            }

            // Process step if detected
            if (steps != 0) {
                int current_multiplier_val = 1;
                
                // Handle step acceleration if enabled
                if (enc->acceleration_enabled) {
                    uint32_t current_time_ms = esp_timer_get_time() / 1000;
                    uint32_t turn_interval_ms = current_time_ms - enc->last_step_time_ms;
                    
                    // Increase step multiplier for rapid turns
                    if (turn_interval_ms < enc->accel_gap_ms && enc->last_step_time_ms != 0) {
                        // Map time gap to multiplier value (smaller gap = larger multiplier)
                        current_multiplier_val = map_value(
                            enc->accel_gap_ms - turn_interval_ms, 
                            1, enc->accel_gap_ms, 
                            1, enc->accel_max_multiplier + 1);
                        
                        // Clamp multiplier to configured min/max values
                        current_multiplier_val = (current_multiplier_val < 1) ? 1 :
                            (current_multiplier_val > enc->accel_max_multiplier) ? 
                            enc->accel_max_multiplier : current_multiplier_val;
                        
                        ESP_LOGD(TAG, "Accel: interval %" PRIu32 "ms, multiplier %d",
                                turn_interval_ms, current_multiplier_val);
                    }
                    enc->last_step_time_ms = current_time_ms;
                } else {
                    // Update timestamp only if acceleration disabled
                    enc->last_step_time_ms = esp_timer_get_time() / 1000;
                }

                // Apply acceleration multiplier to step count
                steps *= current_multiplier_val;
                ESP_LOGD(TAG, "Step detected: %d (after acceleration)", steps);

                // Send step event to output queue
                if (enc->output_queue) {
                    encoder_event_t evt = {.steps = steps};
                    if (xQueueSend(enc->output_queue, &evt, pdMS_TO_TICKS(10)) != pdTRUE) {
                        ESP_LOGW(TAG, "Failed to send encoder event to queue");
                    }
                }
            }
        }
    }
}

/**
 * @brief Create a new encoder instance
 * 
 * @param[in] config Encoder configuration
 * @param[in] output_queue Event output queue
 * @return encoder_handle_t Encoder handle, NULL on failure
 * 
 * @note The output queue must be created before calling this function
 * @warning GPIO pins must have external pull-up resistors if not using internal ones
 */
encoder_handle_t encoder_create(const encoder_config_t *config,
                               QueueHandle_t output_queue) {
    if (!config || !output_queue) {
        ESP_LOGE(TAG, "Invalid arguments: config or output_queue is NULL");
        return NULL;
    }

    encoder_handle_t enc = calloc(1, sizeof(struct encoder_s));
    if (!enc) {
        ESP_LOGE(TAG, "Memory allocation failed for encoder structure");
        return NULL;
    }

    // Initialize encoder configuration
    enc->pin_a = config->pin_a;
    enc->pin_b = config->pin_b;
    enc->output_queue = output_queue;
    enc->half_step_mode = config->half_step_mode;
    enc->acceleration_enabled = config->acceleration_enabled;
    enc->accel_gap_ms = config->accel_gap_ms;
    enc->accel_max_multiplier = config->accel_max_multiplier;
    enc->rotary_state = R_START;
    enc->last_step_time_ms = 0;

    // Configure GPIO pins for input with interrupts
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << enc->pin_a) | (1ULL << enc->pin_b),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };

    if (gpio_config(&io_conf) != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed for pins A:%d, B:%d", 
                enc->pin_a, enc->pin_b);
        free(enc);
        return NULL;
    }

    // Install ISR service (ignore error if already installed)
    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ISR service installation failed: %s", esp_err_to_name(isr_err));
        free(enc);
        return NULL;
    }

    // Add ISR handlers for both encoder pins
    if (gpio_isr_handler_add(enc->pin_a, encoder_isr_handler, enc) != ESP_OK ||
        gpio_isr_handler_add(enc->pin_b, encoder_isr_handler, enc) != ESP_OK) {
        ESP_LOGE(TAG, "ISR handler addition failed for pins A:%d, B:%d", 
                enc->pin_a, enc->pin_b);
        gpio_isr_handler_remove(enc->pin_a);
        gpio_isr_handler_remove(enc->pin_b);
        free(enc);
        return NULL;
    }

    // Create processing task with medium priority
    if (xTaskCreate(encoder_task, "encoder_task", ENCODER_TASK_STACK_SIZE,
                   enc, ENCODER_TASK_PRIORITY, &enc->task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Task creation failed for encoder processing");
        gpio_isr_handler_remove(enc->pin_a);
        gpio_isr_handler_remove(enc->pin_b);
        free(enc);
        return NULL;
    }

    ESP_LOGI(TAG, "Encoder created on pins A:%d, B:%d (%s-step mode, acceleration: %s)",
            enc->pin_a, enc->pin_b, 
            enc->half_step_mode ? "half" : "full",
            enc->acceleration_enabled ? "enabled" : "disabled");
    return enc;
}

/**
 * @brief Delete encoder instance and free resources
 * 
 * @param[in] enc Encoder handle to delete
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG on invalid handle
 * 
 * @note This function does not delete the event queue
 * @warning Ensure no tasks are using the encoder before deletion
 */
esp_err_t encoder_delete(encoder_handle_t enc) {
    if (!enc) {
        ESP_LOGE(TAG, "Invalid encoder handle");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Deleting encoder on pins A:%d, B:%d", enc->pin_a, enc->pin_b);

    // Delete processing task if it exists
    if (enc->task_handle) {
        vTaskDelete(enc->task_handle);
    }

    // Remove ISR handlers
    gpio_isr_handler_remove(enc->pin_a);
    gpio_isr_handler_remove(enc->pin_b);

    // Free encoder structure memory
    free(enc);
    return ESP_OK;
}
#include "encoder.h"
#include "driver/gpio.h"
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "project_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
 * @param x Input value
 * @param in_min Input range minimum
 * @param in_max Input range maximum
 * @param out_min Output range minimum
 * @param out_max Output range maximum
 * @return Mapped value
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
 * @param arg Encoder instance handle
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
 * @param arg Encoder instance handle
 */
static void encoder_task(void *arg) {
    encoder_handle_t enc = (encoder_handle_t)arg;
    uint8_t current_pin_states = 0;

    while (1) {
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            current_pin_states = (gpio_get_level(enc->pin_a) << 1) | 
                               gpio_get_level(enc->pin_b);

            const unsigned char(*current_ttable)[4] =
                enc->half_step_mode ? ttable_half_step : ttable_full_step;

            enc->rotary_state =
                current_ttable[enc->rotary_state & 0x0F][current_pin_states];

            uint8_t direction = enc->rotary_state & 0x30;
            int steps = 0;

            if (direction == DIR_CW) {
                steps = 1;
            } else if (direction == DIR_CCW) {
                steps = -1;
            }

            if (steps != 0) {
                int current_multiplier_val = 1;
                if (enc->acceleration_enabled) {
                    uint32_t current_time_ms = esp_timer_get_time() / 1000;
                    uint32_t turn_interval_ms = current_time_ms - enc->last_step_time_ms;
                    
                    if (turn_interval_ms < enc->accel_gap_ms && enc->last_step_time_ms != 0) {
                        current_multiplier_val = map_value(
                            enc->accel_gap_ms - turn_interval_ms, 
                            1, enc->accel_gap_ms, 
                            1, enc->accel_max_multiplier + 1);
                        
                        current_multiplier_val = (current_multiplier_val < 1) ? 1 :
                            (current_multiplier_val > enc->accel_max_multiplier) ? 
                            enc->accel_max_multiplier : current_multiplier_val;
                        
                        ESP_LOGD(TAG, "Accel: interval %" PRIu32 ", multiplier %d",
                                turn_interval_ms, current_multiplier_val);
                    }
                    enc->last_step_time_ms = current_time_ms;
                } else {
                    enc->last_step_time_ms = esp_timer_get_time() / 1000;
                }

                steps *= current_multiplier_val;
                ESP_LOGD(TAG, "Step detected: %d (after accel)", steps);

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
 * @param config Encoder configuration
 * @param output_queue Event output queue
 * @return encoder_handle_t Encoder handle, NULL on failure
 */
encoder_handle_t encoder_create(const encoder_config_t *config,
                               QueueHandle_t output_queue) {
    if (!config || !output_queue) {
        ESP_LOGE(TAG, "Invalid arguments");
        return NULL;
    }

    encoder_handle_t enc = calloc(1, sizeof(struct encoder_s));
    if (!enc) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }

    enc->pin_a = config->pin_a;
    enc->pin_b = config->pin_b;
    enc->output_queue = output_queue;
    enc->half_step_mode = config->half_step_mode;
    enc->acceleration_enabled = config->acceleration_enabled;
    enc->accel_gap_ms = config->accel_gap_ms;
    enc->accel_max_multiplier = config->accel_max_multiplier;
    enc->rotary_state = R_START;
    enc->last_step_time_ms = 0;

    // Configure GPIO pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << enc->pin_a) | (1ULL << enc->pin_b),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };

    if (gpio_config(&io_conf) != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed");
        free(enc);
        return NULL;
    }

    // Install ISR service
    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ISR service installation failed");
        free(enc);
        return NULL;
    }

    // Add ISR handlers
    if (gpio_isr_handler_add(enc->pin_a, encoder_isr_handler, enc) != ESP_OK ||
        gpio_isr_handler_add(enc->pin_b, encoder_isr_handler, enc) != ESP_OK) {
        ESP_LOGE(TAG, "ISR handler addition failed");
        gpio_isr_handler_remove(enc->pin_a);
        gpio_isr_handler_remove(enc->pin_b);
        free(enc);
        return NULL;
    }

    // Create processing task
    if (xTaskCreate(encoder_task, "encoder_task", ENCODER_TASK_STACK_SIZE,
                   enc, 10, &enc->task_handle) != pdPASS) {
        ESP_LOGE(TAG, "Task creation failed");
        gpio_isr_handler_remove(enc->pin_a);
        gpio_isr_handler_remove(enc->pin_b);
        free(enc);
        return NULL;
    }

    ESP_LOGI(TAG, "Encoder created on pins A:%d, B:%d (%s-step mode)",
            enc->pin_a, enc->pin_b, enc->half_step_mode ? "half" : "full");
    return enc;
}

/**
 * @brief Delete encoder instance
 * @param enc Encoder handle to delete
 * @return esp_err_t Operation status
 */
esp_err_t encoder_delete(encoder_handle_t enc) {
    if (!enc) {
        ESP_LOGE(TAG, "Invalid handle");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Deleting encoder on pins A:%d, B:%d", enc->pin_a, enc->pin_b);

    if (enc->task_handle) {
        vTaskDelete(enc->task_handle);
    }

    gpio_isr_handler_remove(enc->pin_a);
    gpio_isr_handler_remove(enc->pin_b);

    free(enc);
    return ESP_OK;
}
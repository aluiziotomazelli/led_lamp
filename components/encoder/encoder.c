#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "encoder.h" // Created in the previous step

static const char* TAG = "encoder";

// Rotary encoder state table definitions
// From: https://www.best-microcontroller-projects.com/rotary-encoder.html
// (or similar common implementations)

// No complete step yet.
#define R_START 0x0

// Clockwise
#define R_CW_FINAL 0x1
#define R_CW_BEGIN 0x2
#define R_CW_NEXT 0x3

// Counter-clockwise
#define R_CCW_BEGIN 0x4
#define R_CCW_FINAL 0x5
#define R_CCW_NEXT 0x6

// Direction flags in rotary_state
#define DIR_NONE 0x00 // No complete step yet
#define DIR_CW 0x10   // Clockwise step
#define DIR_CCW 0x20  // Counter-clockwise step

// Full step state transition table (example, adjust if different)
static const unsigned char ttable_full_step[7][4] = {
  // R_START (00)
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
  // R_CW_FINAL (01)
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
  // R_CW_BEGIN (02)
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
  // R_CW_NEXT (03)
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
  // R_CCW_BEGIN (04)
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
  // R_CCW_FINAL (05)
  {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
  // R_CCW_NEXT (06)
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};

// Half step state transition table (example, adjust if different)
// This table will have more states if it's a true half-step implementation,
// often doubling the resolution by detecting transitions on all four edges.
// The one from the C++ code seems to be a more optimized full-step variant.
// For now, using the C++ version's "half_step" table directly.

#define R_START 0x0
#define H_CCW_BEGIN 0x1
#define H_CW_BEGIN 0x2
#define H_START_M 0x3
#define H_CW_BEGIN_M 0x4
#define H_CCW_BEGIN_M 0x5
static const unsigned char ttable_half_step[6][4] = {
    // 00                  01              10            11 // BA
    {H_START_M, H_CW_BEGIN, H_CCW_BEGIN, R_START},            // R_START (00)
    {H_START_M | DIR_CCW, R_START, H_CCW_BEGIN, R_START},     // H_CCW_BEGIN
    {H_START_M | DIR_CW, H_CW_BEGIN, R_START, R_START},       // H_CW_BEGIN
    {H_START_M, H_CCW_BEGIN_M, H_CW_BEGIN_M, R_START},        // H_START_M (11)
    {H_START_M, H_START_M, H_CW_BEGIN_M, R_START | DIR_CW},   // H_CW_BEGIN_M
    {H_START_M, H_CCW_BEGIN_M, H_START_M, R_START | DIR_CCW}, // H_CCW_BEGIN_M
};


// Internal structure for an encoder instance
struct encoder_s {
    gpio_num_t pin_a;
    gpio_num_t pin_b;
    QueueHandle_t output_queue;
    TaskHandle_t task_handle;
    volatile uint8_t rotary_state; // Current state of the encoder FSM
    bool half_step_mode;           // True for half-step, false for full-step
    bool acceleration_enabled;     // True to enable dynamic acceleration
    uint16_t accel_gap_ms;         // Time (ms) threshold for acceleration
    uint8_t accel_max_multiplier;  // Max multiplier for steps when accelerating
    volatile uint32_t last_step_time_ms; // Timestamp of the last detected step
    // config_mutex is removed as all parameters are now init-only
};

// Helper function to map a value from one range to another
static inline long map_value(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_min == in_max) {
        return out_min; // Or out_max, or an average, depending on desired behavior
    }
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void IRAM_ATTR encoder_isr_handler(void* arg) {
    encoder_handle_t enc = (encoder_handle_t) arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Notify the dedicated task to process the encoder state change
    vTaskNotifyGiveFromISR(enc->task_handle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void encoder_task(void* arg) {
    encoder_handle_t enc = (encoder_handle_t) arg;
    uint8_t current_pin_states = 0;

    // All configuration parameters (enc->half_step_mode, enc->flip_direction,
    // enc->acceleration_enabled, enc->accel_gap_ms, enc->accel_max_multiplier)
    // are now considered constant after initialization and can be read directly.

    while (1) {
        // Wait for notification from ISR
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            current_pin_states = (gpio_get_level(enc->pin_a) << 1) | gpio_get_level(enc->pin_b);

            const unsigned char (*current_ttable)[4] = enc->half_step_mode ? ttable_half_step : ttable_full_step;

            enc->rotary_state = current_ttable[enc->rotary_state & 0x0F][current_pin_states];

            uint8_t direction = enc->rotary_state & 0x30; // DIR_CW or DIR_CCW
            int steps = 0;

            if (direction == DIR_CW) {
                steps = 1;
            } else if (direction == DIR_CCW) {
                steps = -1;
            }

            if (steps != 0) {
                int current_multiplier_val = 1;
                if (enc->acceleration_enabled) { // Directly use the init-only flag
                    uint32_t current_time_ms = esp_timer_get_time() / 1000;
                    uint32_t turn_interval_ms = current_time_ms - enc->last_step_time_ms;
                    if (turn_interval_ms < enc->accel_gap_ms && enc->last_step_time_ms != 0) {
                        current_multiplier_val = map_value(enc->accel_gap_ms - turn_interval_ms, 1, enc->accel_gap_ms, 1, enc->accel_max_multiplier + 1);
                        if (current_multiplier_val < 1) current_multiplier_val = 1;
                        if (current_multiplier_val > enc->accel_max_multiplier) current_multiplier_val = enc->accel_max_multiplier;
                        ESP_LOGD(TAG, "Accel: interval %lums, multiplier %d", turn_interval_ms, current_multiplier_val);
                    }
                    enc->last_step_time_ms = current_time_ms;
                } else {
                    // Still update last_step_time_ms if a step occurred,
                    // for consistent timing if acceleration were to be re-enabled (though not possible now)
                    // or for other potential future uses of this timestamp.
                    enc->last_step_time_ms = esp_timer_get_time() / 1000;
                }

                steps *= current_multiplier_val;
                ESP_LOGD(TAG, "Step detected: %d (after accel)", steps);

                if (enc->output_queue) {
                    encoder_event_t evt;
                    evt.steps = steps;
                    if (xQueueSend(enc->output_queue, &evt, pdMS_TO_TICKS(10)) != pdTRUE) {
                        ESP_LOGW(TAG, "Failed to send encoder event to queue for pins %d, %d", enc->pin_a, enc->pin_b);
                    }
                } else {
                    ESP_LOGW(TAG, "Output queue not configured for encoder on pins %d, %d", enc->pin_a, enc->pin_b);
                }
            }
            ESP_LOGD(TAG, "Pins: A=%d B=%d -> States: %02X, Rotary State: %02X, Direction: %02X, Raw Steps: %d",
                     gpio_get_level(enc->pin_a), gpio_get_level(enc->pin_b),
                     current_pin_states, enc->rotary_state, direction, (direction == DIR_CW ? 1 : (direction == DIR_CCW ? -1 : 0)));
        }
    }
}

encoder_handle_t encoder_create(const encoder_config_t* config, QueueHandle_t output_queue) {
    if (!config) {
        ESP_LOGE(TAG, "Encoder configuration is NULL");
        return NULL;
    }
    if (!output_queue) { // output_queue is essential now
        ESP_LOGE(TAG, "Output queue is NULL");
        return NULL;
    }

    encoder_handle_t enc = (encoder_handle_t) calloc(1, sizeof(struct encoder_s));
    if (!enc) {
        ESP_LOGE(TAG, "Failed to allocate memory for encoder");
        return NULL;
    }

    enc->pin_a = config->pin_a;
    enc->pin_b = config->pin_b;
    enc->output_queue = output_queue; // Store the output queue

    enc->half_step_mode = config->half_step_mode;
    enc->acceleration_enabled = config->acceleration_enabled;
    enc->accel_gap_ms = config->accel_gap_ms;
    enc->accel_max_multiplier = config->accel_max_multiplier;

    enc->rotary_state = R_START;
    enc->last_step_time_ms = 0;
    // Mutex creation is removed

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << enc->pin_a) | (1ULL << enc->pin_b),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    esp_err_t gpio_cfg_err = gpio_config(&io_conf);
    if (gpio_cfg_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIOs: %s", esp_err_to_name(gpio_cfg_err));
        free(enc); // Free allocated memory for enc struct
        return NULL;
    }

    // Install ISR service if not already installed
    esp_err_t isr_service_err = gpio_install_isr_service(0); // ESP_INTR_FLAG_LEVEL1 by default
    if (isr_service_err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "ISR service already installed.");
    } else if (isr_service_err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(isr_service_err));
        free(enc);
        return NULL;
    }

    if (gpio_isr_handler_add(enc->pin_a, encoder_isr_handler, enc) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for pin_a");
        // gpio_uninstall_isr_service(); // Potentially, but might affect other components
        free(enc);
        return NULL;
    }
    if (gpio_isr_handler_add(enc->pin_b, encoder_isr_handler, enc) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for pin_b");
        gpio_isr_handler_remove(enc->pin_a);
        // gpio_uninstall_isr_service();
        free(enc);
        return NULL;
    }

    BaseType_t task_created = xTaskCreate(encoder_task, "encoder_task", 2048, enc, 10, &enc->task_handle);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create encoder task");
        gpio_isr_handler_remove(enc->pin_a);
        gpio_isr_handler_remove(enc->pin_b);
        // Mutex cleanup is removed
        // gpio_uninstall_isr_service();
        free(enc);
        return NULL;
    }

    ESP_LOGI(TAG, "Encoder created for pins A:%d, B:%d. Mode: %s-step.",
             enc->pin_a, enc->pin_b, enc->half_step_mode ? "half" : "full");
    return enc;
}

esp_err_t encoder_delete(encoder_handle_t enc) {
    if (!enc) {
        ESP_LOGE(TAG, "encoder_delete: handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Deleting encoder for pins A:%d, B:%d", enc->pin_a, enc->pin_b);

    // 1. Delete task
    if (enc->task_handle) {
        vTaskDelete(enc->task_handle);
        enc->task_handle = NULL;
        ESP_LOGD(TAG, "Encoder task deleted for pins A:%d, B:%d", enc->pin_a, enc->pin_b);
    }

    // 2. Remove ISR handlers
    // It's safe to call remove even if not added or already removed, it will return ESP_ERR_INVALID_STATE.
    esp_err_t err_a = gpio_isr_handler_remove(enc->pin_a);
    if (err_a == ESP_OK) {
        ESP_LOGD(TAG, "ISR handler removed for pin_a %d", enc->pin_a);
    } else if (err_a != ESP_ERR_INVALID_STATE) { // ESP_ERR_INVALID_STATE means it was not attached or already removed
        ESP_LOGW(TAG, "Error removing ISR handler for pin_a %d: %s", enc->pin_a, esp_err_to_name(err_a));
    }

    esp_err_t err_b = gpio_isr_handler_remove(enc->pin_b);
    if (err_b == ESP_OK) {
        ESP_LOGD(TAG, "ISR handler removed for pin_b %d", enc->pin_b);
    } else if (err_b != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Error removing ISR handler for pin_b %d: %s", enc->pin_b, esp_err_to_name(err_b));
    }

    // 3. GPIO uninstall ISR service is generally not called here as it's a global service.

    // 4. Mutex Deletion is removed.

    // 5. Free memory
    ESP_LOGI(TAG, "Encoder memory freed for pins A:%d, B:%d", enc->pin_a, enc->pin_b);
    free(enc);

    return ESP_OK;
}

// encoder_set_half_steps, encoder_set_flip_direction, encoder_set_accel_params,
// and encoder_enable_acceleration are all removed as per the plan.
// Configuration for these is now at creation time only via encoder_config_t.

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
static const unsigned char ttable_half_step[7][4] = {
  // R_START
  {R_START,    R_CW_BEGIN,  R_CCW_BEGIN, R_START},
  // R_CW_FINAL
  {R_CW_NEXT,  R_START,     R_CW_FINAL,  R_START | DIR_CW},
  // R_CW_BEGIN
  {R_CW_NEXT,  R_CW_BEGIN,  R_START,     R_START},
  // R_CW_NEXT
  {R_CW_NEXT,  R_CW_BEGIN,  R_CW_FINAL,  R_START},
  // R_CCW_BEGIN
  {R_CCW_NEXT, R_START,     R_CCW_BEGIN, R_START},
  // R_CCW_FINAL
  {R_CCW_NEXT, R_CCW_FINAL, R_START,     R_START | DIR_CCW},
  // R_CCW_NEXT
  {R_CCW_NEXT, R_CCW_FINAL, R_CCW_BEGIN, R_START},
};


// Internal structure for an encoder instance
struct encoder_s {
    gpio_num_t pin_a;
    gpio_num_t pin_b;
    QueueHandle_t output_queue;
    TaskHandle_t task_handle;
    volatile uint8_t rotary_state; // Current state of the encoder FSM
    bool half_step_mode;           // True for half-step, false for full-step
    bool flip_direction;           // True to flip the reported direction
    bool acceleration_enabled;     // True to enable dynamic acceleration
    uint16_t accel_gap_ms;         // Time (ms) threshold for acceleration
    uint8_t accel_max_multiplier;  // Max multiplier for steps when accelerating
    volatile uint32_t last_step_time_ms; // Timestamp of the last detected step
    SemaphoreHandle_t config_mutex; // Mutex for thread-safe access to configuration
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
    bool current_half_step_mode;
    bool current_flip_direction;
    bool current_acceleration_enabled;
    uint16_t current_accel_gap_ms;
    uint8_t current_accel_max_multiplier;

    while (1) {
        // Wait for notification from ISR
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY)) {
            // Read configuration under mutex protection
            if (xSemaphoreTake(enc->config_mutex, portMAX_DELAY) == pdTRUE) {
                current_half_step_mode = enc->half_step_mode;
                current_flip_direction = enc->flip_direction;
                current_acceleration_enabled = enc->acceleration_enabled;
                current_accel_gap_ms = enc->accel_gap_ms;
                current_accel_max_multiplier = enc->accel_max_multiplier;
                xSemaphoreGive(enc->config_mutex);
            } else {
                ESP_LOGE(TAG, "Encoder task failed to take config mutex, using potentially stale config.");
                // Initialize with defaults or last known good to be safe, though portMAX_DELAY should not fail easily
                current_half_step_mode = false; // Default to full step
                current_flip_direction = false;
                current_acceleration_enabled = false;
                current_accel_gap_ms = 100; // Default
                current_accel_max_multiplier = 5; // Default
            }

            current_pin_states = (gpio_get_level(enc->pin_a) << 1) | gpio_get_level(enc->pin_b);

            const unsigned char (*current_ttable)[4] = current_half_step_mode ? ttable_half_step : ttable_full_step;

            enc->rotary_state = current_ttable[enc->rotary_state & 0x0F][current_pin_states];

            uint8_t direction = enc->rotary_state & 0x30; // DIR_CW or DIR_CCW
            int steps = 0;

            if (direction == DIR_CW) {
                steps = 1;
            } else if (direction == DIR_CCW) {
                steps = -1;
            }

            if (current_flip_direction) {
                steps = -steps;
            }

            if (steps != 0) {
                int current_multiplier_val = 1;
                if (current_acceleration_enabled) {
                    uint32_t current_time_ms = esp_timer_get_time() / 1000;
                    uint32_t turn_interval_ms = current_time_ms - enc->last_step_time_ms;
                    if (turn_interval_ms < current_accel_gap_ms && enc->last_step_time_ms != 0) {
                        current_multiplier_val = map_value(current_accel_gap_ms - turn_interval_ms, 1, current_accel_gap_ms, 1, current_accel_max_multiplier + 1);
                        if (current_multiplier_val < 1) current_multiplier_val = 1;
                        if (current_multiplier_val > current_accel_max_multiplier) current_multiplier_val = current_accel_max_multiplier;
                        ESP_LOGD(TAG, "Accel: interval %lums, multiplier %d", turn_interval_ms, current_multiplier_val);
                    }
                    enc->last_step_time_ms = current_time_ms;
                } else {
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
    enc->flip_direction = config->flip_direction;
    enc->acceleration_enabled = config->acceleration_enabled;
    enc->accel_gap_ms = config->accel_gap_ms;
    enc->accel_max_multiplier = config->accel_max_multiplier;

    enc->rotary_state = R_START;
    enc->last_step_time_ms = 0;
    enc->config_mutex = NULL; // Initialize mutex handle

    enc->config_mutex = xSemaphoreCreateMutex();
    if (enc->config_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        free(enc);
        return NULL;
    }

    gpio_config_t io_conf_a = {
        .pin_bit_mask = (1ULL << enc->pin_a),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf_a);

    gpio_config_t io_conf_b = {
        .pin_bit_mask = (1ULL << enc->pin_b),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf_b);

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
        vSemaphoreDelete(enc->config_mutex); // Clean up mutex
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

    // 4. Delete Mutex
    if (enc->config_mutex != NULL) {
        vSemaphoreDelete(enc->config_mutex);
        enc->config_mutex = NULL;
        ESP_LOGD(TAG, "Config mutex deleted for encoder on pins A:%d, B:%d", enc->pin_a, enc->pin_b);
    }

    // 5. Free memory
    ESP_LOGI(TAG, "Encoder memory freed for pins A:%d, B:%d", enc->pin_a, enc->pin_b);
    free(enc);

    return ESP_OK;
}

esp_err_t encoder_set_half_steps(encoder_handle_t enc, bool enable) {
    if (!enc || !enc->config_mutex) {
        ESP_LOGE(TAG, "set_half_steps: Invalid argument (NULL handle or mutex)");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(enc->config_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        enc->half_step_mode = enable;
        ESP_LOGI(TAG, "Encoder on A:%d, B:%d set to %s-step mode.", enc->pin_a, enc->pin_b, enable ? "half" : "full");
        xSemaphoreGive(enc->config_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take config mutex in set_half_steps for A:%d, B:%d", enc->pin_a, enc->pin_b);
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

esp_err_t encoder_set_flip_direction(encoder_handle_t enc, bool flip) {
    if (!enc || !enc->config_mutex) {
        ESP_LOGE(TAG, "set_flip_direction: Invalid argument (NULL handle or mutex)");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(enc->config_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        enc->flip_direction = flip;
        ESP_LOGI(TAG, "Encoder on A:%d, B:%d direction flip set to %s.", enc->pin_a, enc->pin_b, flip ? "true" : "false");
        xSemaphoreGive(enc->config_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take config mutex in set_flip_direction for A:%d, B:%d", enc->pin_a, enc->pin_b);
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

esp_err_t encoder_set_accel_params(encoder_handle_t enc, uint16_t gap_ms, uint8_t multiplier) {
    if (!enc || !enc->config_mutex) {
        ESP_LOGE(TAG, "set_accel_params: Invalid argument (NULL handle or mutex)");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(enc->config_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        enc->accel_gap_ms = gap_ms;
        enc->accel_max_multiplier = multiplier;
        ESP_LOGI(TAG, "Encoder on A:%d, B:%d accel params: gap %ums, multiplier %u.", enc->pin_a, enc->pin_b, gap_ms, multiplier);
        xSemaphoreGive(enc->config_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take config mutex in set_accel_params for A:%d, B:%d", enc->pin_a, enc->pin_b);
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

esp_err_t encoder_enable_acceleration(encoder_handle_t enc, bool enable) {
    if (!enc || !enc->config_mutex) {
        ESP_LOGE(TAG, "enable_acceleration: Invalid argument (NULL handle or mutex)");
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(enc->config_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        enc->acceleration_enabled = enable;
        ESP_LOGI(TAG, "Encoder on A:%d, B:%d acceleration %s.", enc->pin_a, enc->pin_b, enable ? "enabled" : "disabled");
        xSemaphoreGive(enc->config_mutex);
    } else {
        ESP_LOGE(TAG, "Failed to take config mutex in enable_acceleration for A:%d, B:%d", enc->pin_a, enc->pin_b);
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}

#include "touch_button.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "project_config.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
static const char *TAG = "TouchButton";

// Internal structure for a touch button
struct touch_button_s {
    touch_pad_t touch_pad;
    uint16_t threshold;
    QueueHandle_t output_queue;
    TaskHandle_t task_handle;
};

static void touch_button_task(void *param) {
    touch_button_t *btn = (touch_button_t *)param;
    bool pressed = false;
    uint32_t touch_value;

    while (1) {
        // Read the touch sensor value
        touch_pad_read_filtered(btn->touch_pad, &touch_value);

        if (touch_value < btn->threshold && !pressed) {
            pressed = true;
            touch_button_event_t event = {
                .type = TOUCH_BUTTON_PRESS,
                .touch_pad = btn->touch_pad
            };
            xQueueSend(btn->output_queue, &event, pdMS_TO_TICKS(10));
            ESP_LOGD(TAG, "Touch pad %d pressed, value: %d, threshold: %d", btn->touch_pad, touch_value, btn->threshold);
        } else if (touch_value > btn->threshold && pressed) {
            pressed = false;
            touch_button_event_t event = {
                .type = TOUCH_BUTTON_RELEASE,
                .touch_pad = btn->touch_pad
            };
            xQueueSend(btn->output_queue, &event, pdMS_TO_TICKS(10));
            ESP_LOGD(TAG, "Touch pad %d released, value: %d, threshold: %d", btn->touch_pad, touch_value, btn->threshold);
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Check every 50ms
    }
}

touch_button_t *touch_button_create(const touch_button_config_t* config, QueueHandle_t output_queue) {
    if (!config || !output_queue) {
        ESP_LOGE(TAG, "Invalid arguments");
        return NULL;
    }

    touch_button_t *btn = calloc(1, sizeof(touch_button_t));
    if (!btn) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    btn->touch_pad = config->touch_pad;
    btn->output_queue = output_queue;

    // Initialize touch pad driver
    ESP_ERROR_CHECK(touch_pad_init());
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    touch_pad_config(btn->touch_pad, 0);

    // Filter configuration
    touch_filter_config_t filter_info = {
        .mode = TOUCH_PAD_FILTER_IIR_16,
        .debounce_cnt = 1,      // Debounce counter
        .noise_thr = 0,         // Noise threshold
        .jitter_step = 4,       // Jitter filter step
        .smh_lvl = TOUCH_PAD_SMOOTH_IIR_2,
    };
    touch_pad_filter_set_config(&filter_info);
    touch_pad_filter_enable();


    // Calibrate baseline
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for touch pad to stabilize
    uint32_t touch_value_initial = 0;
    touch_pad_read_filtered(btn->touch_pad, &touch_value_initial);
    btn->threshold = (uint16_t)(touch_value_initial * config->threshold_percent);
    ESP_LOGI(TAG, "Touch pad %d initialized. Initial value: %d, Threshold: %d", btn->touch_pad, touch_value_initial, btn->threshold);


    BaseType_t res = xTaskCreate(touch_button_task, "touch_button_task", TOUCH_BUTTON_TASK_STACK_SIZE, btn, 5, &btn->task_handle);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create touch button task");
        free(btn);
        return NULL;
    }

    return btn;
}

void touch_button_delete(touch_button_t *btn) {
    if (btn) {
        if (btn->task_handle) {
            vTaskDelete(btn->task_handle);
        }
        free(btn);
    }
}

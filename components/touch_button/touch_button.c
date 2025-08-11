#include "touch_button.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/touch_sensor.h" // Use the new touch sensor driver
#include "project_config.h"
#include <inttypes.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
static const char *TAG = "TouchButton";

// Internal structure for a touch button
struct touch_button_s {
    touch_sensor_handle_t sens_handle;
    touch_channel_handle_t chan_handle;
    touch_pad_t touch_pad_num;
    QueueHandle_t output_queue;
};

// Callback triggered when a touch pad is activated (pressed)
static bool touch_on_active_cb(touch_sensor_handle_t sens_handle, const touch_active_event_data_t *event, void *user_ctx) {
    touch_button_t *btn = (touch_button_t *)user_ctx;
    if (btn && event->chan_id == btn->touch_pad_num) {
        touch_button_event_t evt = {
            .type = TOUCH_BUTTON_PRESS,
            .touch_pad = btn->touch_pad_num
        };
        xQueueSend(btn->output_queue, &evt, 0);
    }
    return false;
}

// Callback triggered when a touch pad is deactivated (released)
static bool touch_on_inactive_cb(touch_sensor_handle_t sens_handle, const touch_inactive_event_data_t *event, void *user_ctx) {
    touch_button_t *btn = (touch_button_t *)user_ctx;
    if (btn && event->chan_id == btn->touch_pad_num) {
        touch_button_event_t evt = {
            .type = TOUCH_BUTTON_RELEASE,
            .touch_pad = btn->touch_pad_num
        };
        xQueueSend(btn->output_queue, &evt, 0);
    }
    return false;
}

touch_button_t *touch_button_create(const touch_button_config_t* config, QueueHandle_t output_queue) {
    if (!config || !output_queue) {
        ESP_LOGE(TAG, "Invalid arguments");
        return NULL;
    }

    touch_button_t *btn = calloc(1, sizeof(touch_button_t));
    if (!btn) {
        ESP_LOGE(TAG, "Failed to allocate memory for button struct");
        return NULL;
    }

    btn->output_queue = output_queue;
    btn->touch_pad_num = config->touch_pad;

    // --- Start of new driver initialization ---

    // 1. Create a new controller
    touch_sensor_config_t touch_cfg = TOUCH_SENSOR_DEFAULT_BASIC_CONFIG(1, NULL);
    ESP_ERROR_CHECK(touch_sensor_new_controller(&touch_cfg, &btn->sens_handle));

    // 2. Get an initial reading to set the threshold
    // To do this, we need a temporary channel config, then we'll reconfigure it with the threshold
    touch_channel_config_t chan_cfg_temp = {
        .charge_speed = TOUCH_CHARGE_SPEED_7,
        .init_charge_volt = TOUCH_INIT_VOLT_HIGH,
        .abs_active_thresh[0] = 0 // No threshold initially
    };
    ESP_ERROR_CHECK(touch_sensor_new_channel(btn->sens_handle, config->touch_pad, &chan_cfg_temp, &btn->chan_handle));

    // Enable controller to take a reading
    ESP_ERROR_CHECK(touch_sensor_enable(btn->sens_handle));

    // Trigger a one-shot scan to get a baseline value
    uint32_t initial_value[1] = {0};
    ESP_ERROR_CHECK(touch_sensor_trigger_oneshot_scanning(btn->sens_handle, -1)); // Wait forever
    ESP_ERROR_CHECK(touch_channel_read_data(btn->chan_handle, TOUCH_CHAN_DATA_TYPE_RAW, initial_value));

    uint32_t threshold = (uint32_t)(initial_value[0] * config->threshold_percent);
    ESP_LOGI(TAG, "Touch pad %d initialized. Initial value: %" PRIu32 ", Threshold: %" PRIu32, config->touch_pad, initial_value[0], threshold);

    // Now disable and reconfigure the channel with the correct threshold
    ESP_ERROR_CHECK(touch_sensor_disable(btn->sens_handle));
    touch_channel_config_t chan_cfg_final = {
        .charge_speed = TOUCH_CHARGE_SPEED_7,
        .init_charge_volt = TOUCH_INIT_VOLT_HIGH,
        .abs_active_thresh[0] = threshold
    };
    ESP_ERROR_CHECK(touch_sensor_reconfig_channel(btn->chan_handle, &chan_cfg_final));

    // 3. Configure the software filter
    touch_sensor_filter_config_t filter_config = TOUCH_SENSOR_DEFAULT_FILTER_CONFIG();
    ESP_ERROR_CHECK(touch_sensor_config_filter(btn->sens_handle, &filter_config));

    // 4. Register callbacks
    touch_event_callbacks_t callbacks = {
        .on_active = touch_on_active_cb,
        .on_inactive = touch_on_inactive_cb
    };
    ESP_ERROR_CHECK(touch_sensor_register_callbacks(btn->sens_handle, &callbacks, btn));

    // 5. Enable the controller again and start scanning
    ESP_ERROR_CHECK(touch_sensor_enable(btn->sens_handle));
    ESP_ERROR_CHECK(touch_sensor_start_continuous_scanning(btn->sens_handle));

    ESP_LOGI(TAG, "Touch button created on pad %d", config->touch_pad);
    return btn;
}

void touch_button_delete(touch_button_t *btn) {
    if (btn) {
        ESP_ERROR_CHECK(touch_sensor_stop_continuous_scanning(btn->sens_handle));
        ESP_ERROR_CHECK(touch_sensor_disable(btn->sens_handle));
        ESP_ERROR_CHECK(touch_sensor_del_channel(btn->chan_handle));
        ESP_ERROR_CHECK(touch_sensor_del_controller(btn->sens_handle));
        free(btn);
    }
}

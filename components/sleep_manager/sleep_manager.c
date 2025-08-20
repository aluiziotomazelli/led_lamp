#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Set log level for this module, must come before esp_log.h
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
// ESP-IDF system services
#include "esp_log.h"

#include "esp_sleep.h"

#include "sleep_manager.h"
#include "project_config.h"
#include "button.h"
#include "led_driver.h"

static const char *TAG = "PowerManager";
static button_t *button_handle = NULL;

void power_manager_init(button_t *btn_handle) {
    ESP_LOGI(TAG, "Initializing power manager...");

    button_handle = btn_handle;

    // Enable wakeup from GPIOs. The specific pin and trigger level will be
    // configured dynamically before entering sleep.
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    ESP_LOGI(TAG, "Power manager initialized.");
}

void power_manager_enter_sleep(void) {
    ESP_LOGI(TAG, "Preparing for light sleep...");

    // 1. Reset the button state machine to prevent state corruption across sleep cycles.
    if (button_handle) {
        button_reset_state(button_handle);
    }

    // 2. Safely clear the LED strip to prevent random colors during sleep.
    led_driver_prepare_for_sleep();

    ESP_LOGI(TAG, "Arming wakeup sources and entering light sleep...");

    // Configure and enable wakeup sources right before sleeping
    ESP_ERROR_CHECK(gpio_wakeup_enable(BUTTON1_PIN, GPIO_INTR_LOW_LEVEL));

#if IS_SLAVE
    ESP_ERROR_CHECK(esp_sleep_enable_wifi_wakeup());
#endif

    // Enter light sleep
    esp_err_t err = esp_light_sleep_start();

    // Add a small delay RIGHT AFTER waking up. This allows other tasks,
    // like the button ISR and task, to run before we change any GPIO state.
    // This is critical to prevent the wake-up from clearing the button event.
    vTaskDelay(pdMS_TO_TICKS(50));

    // Disable wakeup sources immediately after waking to prevent loops from
    // persistent conditions like a held-down button.
    ESP_ERROR_CHECK(gpio_wakeup_disable(BUTTON1_PIN));

#if IS_SLAVE
    // WIFI wakeup source is automatically disabled after waking up,
    // but we disable it here again to be explicit and safe.
    ESP_ERROR_CHECK(esp_sleep_disable_wifi_wakeup());
#endif

    if (err == ESP_OK) {
        // Determine wakeup reason
        uint64_t wakeup_cause = esp_sleep_get_wakeup_cause();
        if (wakeup_cause & ESP_SLEEP_WAKEUP_GPIO) {
            ESP_LOGI(TAG, "Woke up from light sleep due to GPIO interrupt.");
        } else if (wakeup_cause & ESP_SLEEP_WAKEUP_WIFI) {
            ESP_LOGI(TAG, "Woke up from light sleep due to Wi-Fi event.");
        } else {
            ESP_LOGI(TAG, "Woke up from light sleep due to other reason.");
		}
    } else {
        ESP_LOGE(TAG, "Failed to enter light sleep: %s", esp_err_to_name(err));
    }
}

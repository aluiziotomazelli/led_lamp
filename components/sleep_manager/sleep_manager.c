#include "driver/gpio.h"

// Set log level for this module, must come before esp_log.h
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
// ESP-IDF system services
#include "esp_log.h"

#include "esp_sleep.h"

#include "sleep_manager.h"
#include "project_config.h"

static const char *TAG = "PowerManager";

void power_manager_init(void) {
    ESP_LOGI(TAG, "Initializing power manager...");

    // Enable wakeup from GPIOs
    ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());

    // Configure button pin for wakeup on low level
    // This is the main way the user wakes up the device from MODE_OFF
    ESP_LOGI(TAG, "Configuring GPIO %d as a wakeup source", BUTTON1_PIN);
//    ESP_ERROR_CHECK(gpio_wakeup_enable(BUTTON1_PIN, GPIO_WAKEUP_INTR_LOW));
    ESP_ERROR_CHECK(gpio_wakeup_enable(BUTTON1_PIN, GPIO_INTR_LOW_LEVEL));

#if IS_SLAVE
    // For the slave, we also need to wake up upon receiving a command via Wi-Fi
    ESP_LOGI(TAG, "Device is a SLAVE, enabling Wi-Fi wakeup.");
    ESP_ERROR_CHECK(esp_sleep_enable_wifi_wakeup());
#else
    ESP_LOGI(TAG, "Device is a MASTER, Wi-Fi wakeup is not enabled.");
#endif

    ESP_LOGI(TAG, "Power manager initialized.");
}

void power_manager_enter_sleep(void) {
    ESP_LOGI(TAG, "Entering light sleep...");
    // Flush logs before sleeping
//    esp_log_level_set("*", ESP_LOG_NONE);
//    esp_log_level_set(TAG, ESP_LOG_INFO); // Keep our own tag active if needed

    // Enter light sleep
    esp_err_t err = esp_light_sleep_start();

    // Re-enable logging after waking up
//    esp_log_level_set("*", ESP_LOG_INFO); // Or your default level

    if (err == ESP_OK) {
        // Determine wakeup reason
        uint64_t wakeup_cause = esp_sleep_get_wakeup_cause();
        if (wakeup_cause & ESP_SLEEP_WAKEUP_GPIO) {
            ESP_LOGI(TAG, "Woke up from light sleep due to GPIO interrupt.");
        }
        if (wakeup_cause & ESP_SLEEP_WAKEUP_WIFI) {
            ESP_LOGI(TAG, "Woke up from light sleep due to Wi-Fi event.");
        }
    } else {
        ESP_LOGE(TAG, "Failed to enter light sleep: %s", esp_err_to_name(err));
    }
}

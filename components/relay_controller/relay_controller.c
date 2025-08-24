#include "relay_controller.h"
#include "project_config.h"
#include "driver/gpio.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

static const char *TAG = "RELAY_CTRL";

/// @brief Handle for the one-shot timer used for the delayed-off functionality.
static TimerHandle_t relay_off_timer = NULL;

/// @brief Initialization flag to ensure the component is used only after being initialized.
static bool is_initialized = false;

/**
 * @brief Callback function executed by the FreeRTOS timer.
 * @param xTimer The handle of the timer that expired.
 */
static void relay_off_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Timer expired. Turning relay OFF.");
    gpio_set_level(RELAY_PIN, 0); // Turn the relay OFF
}

/**
 * @brief Initializes the relay controller component.
 */
void relay_controller_init(void) {
    ESP_LOGI(TAG, "Initializing...");

    // Configure the GPIO pin for the relay
    gpio_reset_pin(RELAY_PIN);
    gpio_set_direction(RELAY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RELAY_PIN, 0); // Ensure relay is OFF initially

    // Create the one-shot timer
    relay_off_timer = xTimerCreate("RelayOffTimer",
                                   pdMS_TO_TICKS(RELAY_OFF_DELAY_MS),
                                   pdFALSE, // pdFALSE for one-shot mode
                                   (void *)0,
                                   relay_off_callback);

    if (relay_off_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create the relay-off timer.");
        // Do not set is_initialized to true, so the component remains disabled
        return;
    }

    ESP_LOGI(TAG, "Initialized successfully on GPIO %d with a %dms off-delay.", RELAY_PIN, RELAY_OFF_DELAY_MS);
    is_initialized = true;
}

/**
 * @brief Turns the relay on.
 */
void relay_controller_on(void) {
    if (!is_initialized) {
        return;
    }

    // If the off-timer is running, stop it to cancel the pending turn-off action.
    if (xTimerIsTimerActive(relay_off_timer)) {
        ESP_LOGI(TAG, "Relay ON command received, cancelling pending OFF timer.");
        xTimerStop(relay_off_timer, 0);
    }

    ESP_LOGD(TAG, "Turning relay ON.");
    gpio_set_level(RELAY_PIN, 1); // Turn the relay ON
}

/**
 * @brief Turns the relay off after a configured delay.
 */
void relay_controller_off(void) {
    if (!is_initialized) {
        return;
    }

    ESP_LOGI(TAG, "Relay OFF command received. Starting %dms timer.", RELAY_OFF_DELAY_MS);
    // Start the timer. If it was already running, it will be reset.
    xTimerChangePeriod(relay_off_timer, pdMS_TO_TICKS(RELAY_OFF_DELAY_MS), 0);
}

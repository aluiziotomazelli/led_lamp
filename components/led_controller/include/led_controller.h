#pragma once

#include "esp_err.h"
#include "fsm.h" // Using fsm_state_t to determine rendering behavior
#include <stdint.h>

/**
 * @brief Configuration structure for the LED controller.
 */
typedef struct {
    int gpio_pin;           ///< GPIO pin for the LED strip data line.
    uint32_t led_count;     ///< Number of LEDs in the strip.
    uint32_t task_stack_size; ///< Stack size for the renderer task.
    UBaseType_t task_priority;  ///< Priority for the renderer task.
} led_controller_config_t;

/**
 * @brief Initializes the LED controller and starts the renderer task.
 *
 * This function sets up the RMT peripheral for driving a WS2812-style LED strip
 * and creates a FreeRTOS task to handle LED updates.
 *
 * @param config Pointer to the configuration structure.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t led_controller_init(const led_controller_config_t *config);

/**
 * @brief De-initializes the LED controller.
 *
 * Stops the renderer task and releases resources.
 *
 * @return esp_err_t ESP_OK on success.
 */
esp_err_t led_controller_deinit(void);

/**
 * @brief Sets the global brightness for the LED strip.
 *
 * @param brightness The brightness level (0-255).
 * @return esp_err_t ESP_OK on success.
 */
/**
 * @brief Notifies the renderer task to update the LEDs.
 *
 * This function is lightweight and can be called from any task to signal that the
 * FSM state has changed and the LEDs should be re-rendered.
 */
void led_controller_update_request(void);

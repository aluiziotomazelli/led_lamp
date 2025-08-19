/**
 * @file encoder.h
 * @brief Rotary encoder driver with acceleration support
 * @author Your Name
 * @version 1.0
 */

#pragma once

// System includes
#include "esp_err.h"

// ESP-IDF drivers
#include "driver/gpio.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief Opaque handle for rotary encoder instance
 */
typedef struct encoder_s* encoder_handle_t;

/**
 * @brief Encoder event structure
 */
typedef struct {
    int32_t steps;  ///< Number of steps rotated (positive for CW, negative for CCW)
} encoder_event_t;

/**
 * @brief Encoder configuration parameters
 */
typedef struct {
    gpio_num_t pin_a;            ///< GPIO pin for encoder channel A
    gpio_num_t pin_b;            ///< GPIO pin for encoder channel B
    bool half_step_mode;         ///< True enables higher resolution half-step mode
    bool acceleration_enabled;   ///< Enable dynamic step acceleration
    uint16_t accel_gap_ms;       ///< Time threshold (ms) for max acceleration
    uint8_t accel_max_multiplier; ///< Maximum step multiplier when accelerating
} encoder_config_t;

/**
 * @brief Create a new rotary encoder instance
 * 
 * @param[in] config Encoder configuration parameters
 * @param[in] output_queue FreeRTOS queue for encoder events
 * @return encoder_handle_t Encoder instance handle, NULL on failure
 * 
 * @note The output queue must be created before calling this function
 * @warning GPIO pins must be configured for input with appropriate pull resistors
 */
encoder_handle_t encoder_create(const encoder_config_t* config, QueueHandle_t output_queue);

/**
 * @brief Delete encoder instance and free resources
 * 
 * @param[in] enc Encoder handle to delete
 * @return esp_err_t ESP_OK on success, ESP_ERR_INVALID_ARG on invalid handle
 * 
 * @note This function does not delete the event queue
 * @warning Ensure no tasks are using the encoder before deletion
 */
esp_err_t encoder_delete(encoder_handle_t enc);
#pragma once

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

// Opaque handle for the encoder component
typedef struct encoder_s* encoder_handle_t;

// Event structure for encoder output
typedef struct {
    int32_t steps; // Number of steps rotated (positive for CW, negative for CCW)
    // Future additions: direction, accumulated value, etc.
} encoder_event_t;

// Configuration structure for encoder creation
typedef struct {
    gpio_num_t pin_a;                   // GPIO pin for phase A
    gpio_num_t pin_b;                   // GPIO pin for phase B
    bool half_step_mode;              // True for half-step, false for full-step
    bool flip_direction;              // True to flip the reported direction of rotation
    bool acceleration_enabled;        // True to enable dynamic acceleration
    uint16_t accel_gap_ms;            // Time (ms) between steps to consider for max acceleration
    uint8_t accel_max_multiplier;     // Max multiplier for steps when acceleration is active
} encoder_config_t;

/**
 * @brief Create a new encoder instance.
 *
 * @param config Pointer to the encoder configuration structure.
 * @param output_queue Handle to the queue where encoder events will be sent.
 * @return encoder_handle_t Handle to the created encoder, or NULL on failure.
 */
encoder_handle_t encoder_create(const encoder_config_t* config, QueueHandle_t output_queue);

/**
 * @brief Delete an encoder instance and free associated resources.
 *
 * @param enc Handle to the encoder to delete.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if enc is NULL.
 */
esp_err_t encoder_delete(encoder_handle_t enc);

/**
 * @brief Enable or disable half-step mode.
 *
 * @param enc Handle to the encoder.
 * @param enable True to enable half-step mode, false for full-step.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if enc is NULL.
 */
esp_err_t encoder_set_half_steps(encoder_handle_t enc, bool enable);

/**
 * @brief Set whether to flip the direction of rotation.
 *
 * @param enc Handle to the encoder.
 * @param flip True to flip direction, false for normal.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if enc is NULL.
 */
esp_err_t encoder_set_flip_direction(encoder_handle_t enc, bool flip);

/**
 * @brief Set acceleration parameters.
 *
 * @param enc Handle to the encoder.
 * @param gap_ms Time (ms) between steps to reach maximum acceleration.
 * @param multiplier Maximum step multiplier when acceleration is active.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if enc is NULL.
 */
esp_err_t encoder_set_accel_params(encoder_handle_t enc, uint16_t gap_ms, uint8_t multiplier);

/**
 * @brief Enable or disable rotation acceleration.
 *
 * @param enc Handle to the encoder.
 * @param enable True to enable acceleration, false to disable.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if enc is NULL.
 */
esp_err_t encoder_enable_acceleration(encoder_handle_t enc, bool enable);

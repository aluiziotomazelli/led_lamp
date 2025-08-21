/**
 * @file led_driver.h
 * @brief LED Strip Driver component interface
 * 
 * @details This header provides the interface for controlling LED strips with
 *          color correction capabilities and queue-based data input.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

// System includes
#include <stdint.h>

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief Initialize the LED Strip Driver component.
 *
 * @details This function initializes the physical LED strip driver, creates a task to
 *          listen for pixel data, and starts listening on the provided queue.
 *
 * @param[in] input_queue The queue from which the driver will receive led_strip_t data.
 * 
 * @note The queue must be created before calling this function
 * @warning Do not call this function from an ISR
 */
void led_driver_init(QueueHandle_t input_queue);

/**
 * @brief Set the global color correction for the LED driver.
 *
 * @details This function defines a color correction/white balance to be applied to all
 *          pixels before they are sent to the strip.
 *
 * @param[in] r Red correction value (0-255)
 * @param[in] g Green correction value (0-255)
 * @param[in] b Blue correction value (0-255)
 * 
 * @note Correction values are applied as multipliers to each color channel
 */
void led_driver_set_correction(uint8_t r, uint8_t g, uint8_t b);
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief Initialize the LED Strip Driver component.
 *
 * This function initializes the physical LED strip driver, creates a task to
 * listen for pixel data, and starts listening on the provided queue.
 *
 * @param input_queue The queue from which the driver will receive led_strip_t data.
 */
void led_driver_init(QueueHandle_t input_queue);

/**
 * @brief Set the global color correction for the LED driver.
 *
 * This function defines a color correction/white balance to be applied to all
 * pixels before they are sent to the strip.
 *
 * @param r Red correction value (0-255).
 * @param g Green correction value (0-255).
 * @param b Blue correction value (0-255).
 */
void led_driver_set_correction(uint8_t r, uint8_t g, uint8_t b);
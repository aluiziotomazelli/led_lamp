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

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "led_effects.h"

/**
 * @brief Structure for the abstract LED output data.
 *
 * This is the data packet that the led_controller sends to the output queue.
 * A downstream driver component would receive this and write the colors to the
 * physical LED strip.
 */
typedef struct {
	color_t *pixels;
	uint16_t num_pixels; // Number of pixels in the buffer
	bool is_hsv;
} led_strip_t;

/**
 * @brief Initialize the LED Controller component.
 *
 * This function creates the LED controller task, initializes the effects
 * engine, and sets up the necessary queues. The controller will listen for
 * commands on the `cmd_queue`.
 *
 * @param cmd_queue The queue for receiving `led_command_t` from the FSM.
 * @return QueueHandle_t The handle to the output queue, which will contain
 *         `led_strip_t` data for the LED driver. Returns NULL on failure.
 */
QueueHandle_t led_controller_init(QueueHandle_t cmd_queue);

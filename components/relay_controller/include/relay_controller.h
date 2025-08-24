#pragma once

#include "freertos/FreeRTOS.h"

/**
 * @brief Initializes the relay controller component.
 *
 * Configures the relay GPIO pin and sets up the necessary timer.
 * This must be called before any other relay_controller functions.
 */
void relay_controller_init(void);

/**
 * @brief Turns the relay on.
 *
 * If a pending off-timer is running, it will be cancelled.
 */
void relay_controller_on(void);

/**
 * @brief Turns the relay off after a configured delay.
 *
 * This function starts a non-blocking timer. The relay will be turned
 * off when the timer expires.
 */
void relay_controller_off(void);
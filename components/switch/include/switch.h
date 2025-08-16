#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief Opaque pointer to a switch instance.
 */
typedef struct switch_s *switch_t;

/**
 * @brief Switch configuration structure.
 */
typedef struct {
    gpio_num_t pin;       ///< GPIO pin for the switch.
    bool active_low;      ///< True if the switch is active low (connects to GND when closed).
    uint16_t debounce_ms; ///< Debounce time in milliseconds.
} switch_config_t;

/**
 * @brief Switch event data structure, sent to the queue.
 */
typedef struct {
    gpio_num_t pin;  ///< The GPIO pin number of the switch that triggered the event.
    bool is_closed;  ///< true if the switch is in the "closed" position, false if "open".
} switch_event_t;

/**
 * @brief Create a new switch instance.
 *
 * This function allocates and initializes a new switch component.
 *
 * @param config Pointer to the switch configuration structure.
 * @param queue The queue to which switch events will be sent.
 * @return A handle to the created switch instance (switch_t), or NULL on failure.
 */
switch_t switch_create(const switch_config_t *config, QueueHandle_t queue);

/**
 * @brief Delete a switch instance and free its resources.
 *
 * @param sw The switch handle to delete.
 */
void switch_delete(switch_t sw);

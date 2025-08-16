#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

/**
 * @brief Switch event data structure
 *
 * Represents the state of a physical switch.
 */
typedef struct {
    gpio_num_t pin;  ///< The GPIO pin number of the switch that triggered the event.
    bool is_closed;  ///< true if the switch is in the "closed" position, false if "open"
} switch_event_t;

#pragma once

#include <stdbool.h>

/**
 * @brief Switch event data structure
 *
 * Represents the state of the physical switch.
 */
typedef struct {
    bool is_closed;  ///< true if the switch is in the "closed" position, false if "open"
} switch_event_t;

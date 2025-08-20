#pragma once

#include "button.h"

/**
 * @brief Provides access to the global button handle.
 *
 * @return A pointer to the button_t handle initialized in main.c.
 */
button_t* main_get_button_handle(void);

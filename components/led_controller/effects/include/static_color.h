#pragma once

#include "led_effects.h" // For effect_param_t, color_t

// Declare the parameter array for the static_color effect
extern effect_param_t params_static_color[];

/**
 * @brief Runs the static_color effect.
 *
 * @param params      Array of parameters for the effect.
 * @param num_params  Number of parameters.
 * @param brightness  Master brightness.
 * @param time_ms     Current time for animation.
 * @param pixels      Output pixel buffer.
 * @param num_pixels  Number of pixels.
 */
void run_static_color(const effect_param_t *params, uint8_t num_params,
                      uint8_t brightness, uint64_t time_ms, color_t *pixels,
                      uint16_t num_pixels);

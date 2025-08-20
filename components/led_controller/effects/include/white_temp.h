#pragma once

#include "led_effects.h" // For effect_param_t, color_t

// Declare the parameter array for the white_temp effect
extern effect_param_t params_white_temp[];

/**
 * @brief Runs the white_temp effect.
 *
 * @param params      Array of parameters for the effect.
 * @param num_params  Number of parameters.
 * @param brightness  Master brightness.
 * @param time_ms     Current time for animation.
 * @param pixels      Output pixel buffer.
 * @param num_pixels  Number of pixels.
 */
void run_white_temp(const effect_param_t *params, uint8_t num_params,
                    uint8_t brightness, uint64_t time_ms, color_t *pixels,
                    uint16_t num_pixels);

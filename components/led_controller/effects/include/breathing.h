#pragma once

#include "led_effects.h" // For effect_param_t, color_t

static effect_param_t params_breathing[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 5,
	 .min_value = 1,
	 .max_value = 100,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 5},
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 200,
	 .min_value = 0,
	 .max_value = 359,
	 .step = 1,
	 .is_wrap = true,
	 .default_value = 200},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 255,
	 .min_value = 0,
	 .max_value = 255,
	 .step = 5,
	 .is_wrap = false,
	 .default_value = 255},
};

/**
 * @brief Runs the breathing effect.
 *
 * @param params      Array of parameters for the effect.
 * @param num_params  Number of parameters.
 * @param brightness  Master brightness.
 * @param time_ms     Current time for animation.
 * @param pixels      Output pixel buffer.
 * @param num_pixels  Number of pixels.
 */
void run_breathing(const effect_param_t *params, uint8_t num_params,
                   uint8_t brightness, uint64_t time_ms, color_t *pixels,
                   uint16_t num_pixels);

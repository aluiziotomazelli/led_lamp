#pragma once

#include "led_effects.h" // For effect_param_t, color_t

static effect_param_t params_random_twinkle[] = {
	{.name = "Probability",
	 .type = PARAM_TYPE_VALUE,
	 .value = 20,
	 .min_value = 1,
	 .max_value = 100,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 20},
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 3,
	 .min_value = 1,
	 .max_value = 50,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 3},
	{.name = "Max Twinkles",
	 .type = PARAM_TYPE_VALUE,
	 .value = 8,
	 .min_value = 1,
	 .max_value = 50,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 10},
	{.name = "Palette",
	 .type = PARAM_TYPE_VALUE,
	 .value = 0,
	 .min_value = 0,
	 .max_value = 3,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 0},
};

/**
 * @brief Runs the random_twinkle effect.
 *
 * @param params      Array of parameters for the effect.
 * @param num_params  Number of parameters.
 * @param brightness  Master brightness.
 * @param time_ms     Current time for animation.
 * @param pixels      Output pixel buffer.
 * @param num_pixels  Number of pixels.
 */
void run_random_twinkle(const effect_param_t *params, uint8_t num_params,
                        uint8_t brightness, uint64_t time_ms, color_t *pixels,
                        uint16_t num_pixels);

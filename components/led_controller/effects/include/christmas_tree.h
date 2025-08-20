#pragma once

#include "led_effects.h" // For effect_param_t, color_t

static effect_param_t params_christmas_tree[] = {
	{.name = "Twinkle Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 5,
	 .min_value = 1,
	 .max_value = 50,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 5},
	{.name = "Twinkles",
	 .type = PARAM_TYPE_VALUE,
	 .value = 4,
	 .min_value = 0,
	 .max_value = 20, // Max 20 twinkles
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 4},
};

/**
 * @brief Runs the christmas_tree effect.
 *
 * @param params      Array of parameters for the effect.
 * @param num_params  Number of parameters.
 * @param brightness  Master brightness.
 * @param time_ms     Current time for animation.
 * @param pixels      Output pixel buffer.
 * @param num_pixels  Number of pixels.
 */
void run_christmas_tree(const effect_param_t *params, uint8_t num_params,
                        uint8_t brightness, uint64_t time_ms, color_t *pixels,
                        uint16_t num_pixels);

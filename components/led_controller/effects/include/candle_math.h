#pragma once

#include "led_effects.h" // For effect_param_t, color_t

static effect_param_t params_candle_math[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 10,
	 .min_value = 1,
	 .max_value = 100,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 10},
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 260,
	 .min_value = 0,
	 .max_value = 359,
	 .step = 1,
	 .is_wrap = true,
	 .default_value = 260},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 255,
	 .min_value = 0,
	 .max_value = 255,
	 .step = 5,
	 .is_wrap = false,
	 .default_value = 255},
	{.name = "Segments",
	 .type = PARAM_TYPE_VALUE,
	 .value = 4,
	 .min_value = 1,
	 .max_value = NUM_LEDS,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 4},
	{.name = "Intensity",
	 .type = PARAM_TYPE_VALUE,
	 .value = 30,
	 .min_value = 0,
	 .max_value = 100,
	 .step = 5,
	 .is_wrap = false,
	 .default_value = 30},
	{.name = "Dip Prob",
	 .type = PARAM_TYPE_VALUE,
	 .value = 3,
	 .min_value = 0,
	 .max_value = 100,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 3},
};

/**
 * @brief Runs the candle_math effect.
 *
 * @param params      Array of parameters for the effect.
 * @param num_params  Number of parameters.
 * @param brightness  Master brightness.
 * @param time_ms     Current time for animation.
 * @param pixels      Output pixel buffer.
 * @param num_pixels  Number of pixels.
 */
void run_candle_math(const effect_param_t *params, uint8_t num_params,
                     uint8_t brightness, uint64_t time_ms, color_t *pixels,
                     uint16_t num_pixels);

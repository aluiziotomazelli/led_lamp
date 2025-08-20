#include "led_effects.h"
#include <math.h>
#include <stdlib.h>

#include "hsv2rgb.h"
#include "include/ef_candle_math.h"


/* --- Effect: Candle Math --- */

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


static void run_candle_math(const effect_param_t *params, uint8_t num_params,
							uint8_t brightness, uint64_t time_ms,
							color_t *pixels, uint16_t num_pixels) {

	static candle_effect_t *candle_effect = NULL;
	static uint16_t last_num_pixels = 0;
	static uint16_t last_num_zones = 0;
	static uint64_t last_time_ms = 0;

	// Extract parameters
	uint8_t p_speed = params[0].value;
	uint16_t p_hue = params[1].value;
	uint8_t p_sat = params[2].value;
	uint8_t p_segments = params[3].value;
	uint8_t p_intensity = params[4].value;
	uint8_t p_dip_prob = params[5].value;

	if (p_segments == 0)
		p_segments = 1;
	if (p_segments > num_pixels)
		p_segments = num_pixels;

	// Re-initialize if number of pixels or zones changes
	if (candle_effect == NULL || num_pixels != last_num_pixels ||
		p_segments != last_num_zones) {
		if (candle_effect != NULL) {
			candle_effect_deinit(candle_effect);
		}

		candle_config_t config = {
			.num_zones = p_segments,
			.leds_per_zone =
				(p_segments > 0) ? (num_pixels / p_segments) : num_pixels,
			.flicker_speed = 0.05f,
			.dip_probability = 0.01f,
			.recovery_rate = 0.1f,
			.min_brightness = 10.0f,
			.max_brightness = 100.0f,
			.base_brightness = 70.0f,
			.flicker_intensity = 0.2f,
			.base_hue = 30,
			.base_sat = 255,
		};
		candle_effect = candle_effect_init(&config);
		last_num_pixels = num_pixels;
		last_num_zones = p_segments;
		last_time_ms =
			time_ms; // Reset time to avoid large delta on first frame
	}

	// Update config with current parameters
	candle_effect->config.flicker_speed = (float)p_speed / 20.0f;
	candle_effect->config.base_hue = p_hue;
	candle_effect->config.base_sat = p_sat;
	candle_effect->config.flicker_intensity = (float)p_intensity / 100.0f;
	candle_effect->config.dip_probability = (float)p_dip_prob / 1000.0f;
	candle_effect->config.leds_per_zone =
		(p_segments > 0) ? (num_pixels / p_segments) : num_pixels;

	// Calculate delta time for the update function
	float delta_time = 0.0f;
	if (time_ms > last_time_ms) {
		delta_time = (time_ms - last_time_ms) / 1000.0f;
	}
	last_time_ms = time_ms;

	// Run the candle simulation
	candle_effect_update(candle_effect, delta_time, pixels, num_pixels);

	// Apply master brightness
	float master_brightness_multiplier = (float)brightness / 255.0f;
	for (uint16_t i = 0; i < num_pixels; i++) {
		uint16_t original_v = pixels[i].hsv.v;
		pixels[i].hsv.v =
			(uint8_t)((float)original_v * master_brightness_multiplier);
	}
}



/* =========================== */
/* --- List of all effects --- */
/* =========================== */

#include "ef_candle.h"
effect_t effect_candle = {.name = "Candle",
						  .run = run_candle,
						  .color_mode = COLOR_MODE_HSV,
						  .params = params_candle,
						  .num_params =
							  sizeof(params_candle) / sizeof(effect_param_t),
						  .is_dynamic = true};


#include "ef_white_temp.h"
effect_t effect_white_temp = {.name = "White Temp",
							  .run = run_white_temp,
							  .color_mode = COLOR_MODE_RGB,
							  .params = params_white_temp,
							  .num_params = sizeof(params_white_temp) /
											sizeof(effect_param_t),
							  .is_dynamic = false};
					
							  
#include "ef_random_twinkle.h"
effect_t effect_random_twinkle = {.name = "Random Twinkle",
								  .run = run_random_twinkle,
								  .color_mode = COLOR_MODE_HSV,
								  .params = params_random_twinkle,
								  .num_params = sizeof(params_random_twinkle) /
												sizeof(effect_param_t),
								  .is_dynamic = true};


#include "ef_christmas_twinkle.h"
effect_t effect_christmas_twinkle = {
	.name = "Christmas Twinkle",
	.run = run_christmas_twinkle,
	.color_mode = COLOR_MODE_RGB,
	.params = params_christmas_twinkle,
	.num_params = sizeof(params_christmas_twinkle) / sizeof(effect_param_t),
	.is_dynamic = true};


#include "ef_static_color.h"
effect_t effect_static_color = {.name = "Static Color",
								.run = run_static_color,
								.color_mode = COLOR_MODE_HSV,
								.params = params_static_color,
								.num_params = sizeof(params_static_color) /
											  sizeof(effect_param_t),
								.is_dynamic = false};

#include "ef_breathing.h"
effect_t effect_breathing = {.name = "Breathing",
							 .run = run_breathing,
							 .color_mode = COLOR_MODE_HSV,
							 .params = params_breathing,
							 .num_params = sizeof(params_breathing) /
										   sizeof(effect_param_t),
							 .is_dynamic = true};


effect_t effect_candle_math = {.name = "Candle Math",
							   .run = run_candle_math,
							   .color_mode = COLOR_MODE_HSV,
							   .params = params_candle_math,
							   .num_params = sizeof(params_candle_math) /
											 sizeof(effect_param_t),
							   .is_dynamic = true};


#include "ef_christmas_tree.h"
effect_t effect_christmas_tree = {.name = "Christmas",
							 .run = run_christmas_tree,
							 .color_mode = COLOR_MODE_HSV,
							 .params = params_christmas_tree,
							 .num_params = sizeof(params_christmas_tree) /
										   sizeof(effect_param_t),
							 .is_dynamic = true};

effect_t *effects[] = {&effect_candle,
					   &effect_white_temp,
					   &effect_static_color,
					   &effect_christmas_tree,
					   &effect_candle_math,
					   &effect_christmas_twinkle,
					   &effect_random_twinkle};

const uint8_t effects_count = sizeof(effects) / sizeof(effects[0]);

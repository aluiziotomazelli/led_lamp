#include "led_effects.h" // For color_t, effect_param_t, etc.
#include <math.h>

/* --- Effect: Breathing --- */

effect_param_t params_breathing[] = {
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

void run_breathing(const effect_param_t *params, uint8_t num_params,
						  uint8_t brightness, uint64_t time_ms, color_t *pixels,
						  uint16_t num_pixels) {
	float speed = (float)params[0].value / 20.0f;
	uint16_t hue = params[1].value;
	uint8_t saturation = params[2].value;

	// Calculate brightness using a sine wave for a smooth breathing effect
	// The wave oscillates between 0 and 1.
	float wave = (sinf(time_ms * speed / 1000.0f) + 1.0f) / 2.0f;

	// Scale brightness from 0 to 255 for the HSV value
	uint8_t hsv_v = (uint8_t)(wave * 255.0f);

	hsv_t hsv = {.h = hue, .s = saturation, .v = hsv_v};

	for (uint16_t i = 0; i < num_pixels; i++) {
		pixels[i].hsv = hsv;
	}
}

#include "led_effects.h"
#include <math.h>

/* --- Effect: Breathing --- */

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

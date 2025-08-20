#include "led_effects.h" // For color_t, effect_param_t, etc.
#include <stdint.h>

void run_static_color(const effect_param_t *params, uint8_t num_params,
							 uint8_t brightness, uint64_t time_ms,
							 color_t *pixels, uint16_t num_pixels) {
	hsv_t hsv = {
		.h = params[0].value,
		.s = params[1].value,
		.v = brightness // Full brightness, master brightness is applied in controller
	};

	for (uint16_t i = 0; i < num_pixels; i++) {
		pixels[i].hsv = hsv;
	}
}

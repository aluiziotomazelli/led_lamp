#pragma once

// System includes
#include <stdint.h>

// Project specific headers
#include "led_effects.h" // For color_t


static effect_param_t params_static_color[] = {
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 250,
	 .min_value = 0,
	 .max_value = 359,
	 .step = 1,
	 .is_wrap = true,
	 .default_value = 250},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 230,
	 .min_value = 0,
	 .max_value = 255,
	 .step = 5,
	 .is_wrap = false,
	 .default_value = 230},
};

static void run_static_color(const effect_param_t *params, uint8_t num_params,
							 uint8_t brightness, uint64_t time_ms,
							 color_t *pixels, uint16_t num_pixels) {
	hsv_t hsv = {
		.h = params[0].value,
		.s = params[1].value,
		.v = brightness // Full brightness, master brightness is applied in controller
	};
	//        ESP_LOGI("Cor estatica", "HSV, H = %d, S = %d, V = %d", color.h,
	//        color.s, color.v);

//	rgb_t rgb;
//	hsv_to_rgb_spectrum_deg(color.h, color.s, color.v, &rgb.r, &rgb.g, &rgb.b);

	for (uint16_t i = 0; i < num_pixels; i++) {
		pixels[i].hsv = hsv;
	}
}
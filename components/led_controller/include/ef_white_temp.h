#pragma once

// System includes
#include <stdint.h>

// Project specific headers
#include "led_effects.h" // For color_t


/* --- Effect: White Temp --- */

static effect_param_t params_white_temp[] = {
	{.name = "Temperature",
	 .type = PARAM_TYPE_VALUE,
	 .value = 0,
	 .min_value = 0,
	 .max_value = 5,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 0},
};

static void run_white_temp(const effect_param_t *params, uint8_t num_params,
						   uint8_t brightness, uint64_t time_ms,
						   color_t *pixels, uint16_t num_pixels) {

	int16_t temp_index = params[0].value;
	rgb_t rgb;

	switch (temp_index) {
	case 0: // Bem quente
		rgb = (rgb_t){255, 130, 30};
		break;
	case 1: // quente
		rgb = (rgb_t){255, 140, 50};
		break;
	case 2: // neutro
		rgb = (rgb_t){255, 197, 143};
		break;
	case 3: // frio
		rgb = (rgb_t){255, 214, 170};
		break;
	case 4: // mais frio
		rgb = (rgb_t){255, 255, 255};
		break;
	case 5: // gelado
		rgb = (rgb_t){201, 226, 255};
		break;
	default: // fallback to neutral
		rgb = (rgb_t){255, 197, 143};
		break;
	}

	for (uint16_t i = 0; i < num_pixels; i++) {
		pixels[i].rgb = rgb;
	}
}


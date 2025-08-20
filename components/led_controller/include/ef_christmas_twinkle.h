#pragma once

// System includes
#include <stdint.h>

// Project specific headers
#include "led_effects.h" // For color_t

/* --- Effect: Christmas Twinkle --- */




typedef struct {
	uint8_t inc;  // velocidade
	int dim;	  // fase (-255..255)
	color_t base; // cor base (em RGB)
} christmas_twinkle_t;

static christmas_twinkle_t *twinkle_states = NULL;
static uint16_t twinkle_num_leds = 0;

static void choose_new_color(christmas_twinkle_t *s) {
	int choice = rand() % 3;
	switch (choice) {
	case 0: // vermelho
		s->base.rgb.r = 255;
		s->base.rgb.g = 0;
		s->base.rgb.b = 18;
		break;
	case 1: // verde
		s->base.rgb.r = 0;
		s->base.rgb.g = 179;
		s->base.rgb.b = 44;
		break;
	default: // branco
		s->base.rgb.r = 255;
		s->base.rgb.g = 255;
		s->base.rgb.b = 255;
		break;
	}
}

static effect_param_t params_christmas_twinkle[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 10,
	 .min_value = 1,
	 .max_value = 50,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 10},
	{.name = "Density",
	 .type = PARAM_TYPE_VALUE,
	 .value = 10,
	 .min_value = 1,
	 .max_value = 20,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 10},
};

static void run_christmas_twinkle(const effect_param_t *params,
								  uint8_t num_params, uint8_t brightness,
								  uint64_t time_ms, color_t *pixels,
								  uint16_t num_pixels) {
	uint8_t speed = params[0].value;
	uint8_t density =
		params[1].value; // controla quantos LEDs piscam ao mesmo tempo

	// inicialização de estado (se mudou número de LEDs)
	if (twinkle_states == NULL || twinkle_num_leds != num_pixels) {
		if (twinkle_states)
			free(twinkle_states);
		twinkle_states = calloc(num_pixels, sizeof(christmas_twinkle_t));
		twinkle_num_leds = num_pixels;

		for (uint16_t i = 0; i < num_pixels; i++) {
			twinkle_states[i].inc = (rand() % 8) + 1;
			twinkle_states[i].dim = (rand() % 511) - 255;
			choose_new_color(&twinkle_states[i]);
		}
	}

	// preenche pixels
	for (uint16_t i = 0; i < num_pixels; i++) {
		christmas_twinkle_t *s = &twinkle_states[i];

		int brightness_local = 255 - abs(s->dim); // 0..255

		// "density": só ativa brilho se abaixo do limite (simulação de menos
		// LEDs piscando)
		if ((i % (20 / density + 1)) != 0) {
			brightness_local = brightness_local / 4; // apaga mais rápido
		}

		pixels[i].rgb.r = (s->base.rgb.r * brightness_local) / 255;
		pixels[i].rgb.g = (s->base.rgb.g * brightness_local) / 255;
		pixels[i].rgb.b = (s->base.rgb.b * brightness_local) / 255;

		// avança estado
		s->dim += (s->inc * speed) / 10;
		if (s->dim > 255) {
			s->dim = -255;
			choose_new_color(s);
		}
	}
}

#pragma once

// System includes
#include <stdint.h>

// Project specific headers
#include "led_effects.h" // For color_t


/* --- Effect: Random Twinkle --- */

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
	 .value = 10,
	 .min_value = 1,
	 .max_value = 50,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 10},
	{.name = "Max Twinkles",
	 .type = PARAM_TYPE_VALUE,
	 .value = 8,
	 .min_value = 1,
	 .max_value = 30,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 8},
	{.name = "Palette",
	 .type = PARAM_TYPE_VALUE,
	 .value = 0,
	 .min_value = 0,
	 .max_value = 3,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 0},
};

static void pick_twinkle_color(uint8_t palette, color_t *c) {
	// Definições HSV para cada cor
	// Hue em graus (0..360), saturação/valor 0..255
	switch (palette) {
		case 0: { // somente dourado
			c->hsv.h = 40;  // dourado
			c->hsv.s = 240;
			c->hsv.v = 255;
			break;
		}
		case 1: { // dourado + branco
			int r = rand() % 2;
			if (r == 0) { // dourado
				c->hsv.h = 40; c->hsv.s = 240; c->hsv.v = 255;
			} else {      // branco
				c->hsv.h = 0;  c->hsv.s = 0;   c->hsv.v = 255;
			}
			break;
		}
		case 2: { // dourado + branco + vermelho
			int r = rand() % 3;
			if (r == 0) { // dourado
				c->hsv.h = 40; c->hsv.s = 240; c->hsv.v = 255;
			} else if (r == 1) { // branco
				c->hsv.h = 0;  c->hsv.s = 0;   c->hsv.v = 255;
			} else { // vermelho
				c->hsv.h = 0;  c->hsv.s = 255; c->hsv.v = 255;
			}
			break;
		}
		default: { // dourado + branco + vermelho + verde
			int r = rand() % 4;
			if (r == 0) { // dourado
				c->hsv.h = 40; c->hsv.s = 240; c->hsv.v = 255;
			} else if (r == 1) { // branco
				c->hsv.h = 0;  c->hsv.s = 0;   c->hsv.v = 255;
			} else if (r == 2) { // vermelho
				c->hsv.h = 0;  c->hsv.s = 255; c->hsv.v = 255;
			} else { // verde
				c->hsv.h = 120; c->hsv.s = 255; c->hsv.v = 255;
			}
			break;
		}
	}
}


static void run_random_twinkle(const effect_param_t *params, uint8_t num_params,
							   uint8_t brightness, uint64_t time_ms,
							   color_t *pixels, uint16_t num_pixels) {
	typedef struct {
		int phase;       // -255..255, controla o fade (triangular)
		bool active;     // se o LED está piscando agora
		uint8_t cooldown;// frames de espera para rearmar
		color_t color; // <--- nova cor persistente do twinkle
	} random_twinkle_t;

	static random_twinkle_t *twinkle_state = NULL;
	static uint16_t twinkle_num_leds2 = 0;

	uint8_t probability = params[0].value; // 1..100 (%)
	uint8_t speed       = params[1].value; // 1..50
	uint8_t max_active  = params[2].value; // 1..20

	// (re)inicializa estado se mudou o número de LEDs
	if (twinkle_state == NULL || twinkle_num_leds2 != num_pixels) {
		if (twinkle_state) free(twinkle_state);
		twinkle_state = calloc(num_pixels, sizeof(random_twinkle_t));
		twinkle_num_leds2 = num_pixels;
		for (uint16_t i = 0; i < num_pixels; i++) {
			twinkle_state[i].phase = -255;
			twinkle_state[i].active = false;
			twinkle_state[i].cooldown = 0;
		}
	}

	// 1) Limpa a imagem deste frame (fundo apagado)
	for (uint16_t i = 0; i < num_pixels; i++) {
		pixels[i].hsv.h = 0;
		pixels[i].hsv.s = 0;
		pixels[i].hsv.v = 0;
	}

	// 2) Atualiza os twinkles ativos e conta quantos seguem ativos
	uint16_t active_count = 0;
	for (uint16_t i = 0; i < num_pixels; i++) {
		random_twinkle_t *s = &twinkle_state[i];

		if (s->active) {
			int b = 255 - abs(s->phase);  // 0..255 brilho local
			pixels[i].hsv.h = s->color.hsv.h;         // dourado
			pixels[i].hsv.s = s->color.hsv.s;
			pixels[i].hsv.v = (b * s->color.hsv.v) /255 ;

			// avança fase
			s->phase += speed;
			if (s->phase > 255) {
				// terminou: desativa e aplica um cooldown curto
				s->active = false;
				s->phase = -255;
				s->cooldown = 2 + (rand() % 4); // 2..5 frames
			} else {
				active_count++;
			}
		} else {
			// conta regressiva do cooldown
			if (s->cooldown > 0) s->cooldown--;
		}
	}

	// 3) Spawns novos twinkles de forma aleatória global (sem viés de índice)
	if (active_count < max_active && probability > 0) {
		uint16_t capacity = max_active - active_count;

		// estimativa de quantos novos twinkles queremos neste frame
		uint16_t inactive = num_pixels - active_count;
		uint16_t target_new = (inactive * probability + 99) / 100; // arredonda pra cima
		if (target_new > capacity) target_new = capacity;
		if (target_new == 0 && capacity > 0 && (rand() % 100) < probability) {
			// chance mínima de pelo menos 1, para não "morrer" com prob baixa
			target_new = 1;
		}

		// ativa 'target_new' LEDs escolhendo índices aleatórios, respeitando cooldown
		uint16_t spawned = 0;
		uint32_t max_tries = target_new * 8 + 16; // limita tentativas pra evitar loop longo
		while (spawned < target_new && max_tries--) {
			uint16_t idx = (num_pixels > 0) ? (rand() % num_pixels) : 0;
			random_twinkle_t *s = &twinkle_state[idx];
			if (!s->active && s->cooldown == 0) {
				s->active = true;
				s->phase = -255;
				pick_twinkle_color(params[3].value, &s->color); // usa Palette
				spawned++;
			}
		}
	}
}
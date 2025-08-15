#include "led_effects.h"
#include "esp_random.h"
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include <math.h> // For fmod in hsv_to_rgb
#include <stdlib.h>
/**
 * @brief Converte HSV para RGB, replicando a lógica do driver.
 *
 * @param hsv Cor no formato HSV (hue: 0-359, saturation: 0-100, value: 0-100)
 * @param rgb Ponteiro para a estrutura RGB de saída (0-255 para cada canal)
 */
static void hsv_to_rgb(hsv_t hsv, rgb_t *rgb) {
	// Normaliza os valores (garante que estão dentro dos limites)
	uint16_t hue = hsv.h % 360;
	uint8_t saturation = (hsv.s > 100) ? 100 : hsv.s;
	uint8_t value = (hsv.v > 100) ? 100 : hsv.v;

	// Converte saturation e value para 0-255 (faixa do driver)
	uint32_t rgb_max = value * 255 / 100;
	uint32_t rgb_min = rgb_max * (255 - saturation) / 255;

	// Calcula componentes RGB
	uint32_t i = hue / 60;
	uint32_t diff = hue % 60;
	uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

	switch (i) {
	case 0:
		rgb->r = rgb_max;
		rgb->g = rgb_min + rgb_adj;
		rgb->b = rgb_min;
		break;
	case 1:
		rgb->r = rgb_max - rgb_adj;
		rgb->g = rgb_max;
		rgb->b = rgb_min;
		break;
	case 2:
		rgb->r = rgb_min;
		rgb->g = rgb_max;
		rgb->b = rgb_min + rgb_adj;
		break;
	case 3:
		rgb->r = rgb_min;
		rgb->g = rgb_max - rgb_adj;
		rgb->b = rgb_max;
		break;
	case 4:
		rgb->r = rgb_min + rgb_adj;
		rgb->g = rgb_min;
		rgb->b = rgb_max;
		break;
	default:
		rgb->r = rgb_max;
		rgb->g = rgb_min;
		rgb->b = rgb_max - rgb_adj;
		break;
	}
}

/* --- Effect: Static Color --- */

static effect_param_t params_static_color[] = {
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 50,
	 .min_value = 0,
	 .max_value = 359,
	 .step = 1},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 100,
	 .min_value = 0,
	 .max_value = 254,
	 .step = 5},
};

static void run_static_color(const effect_param_t *params, uint8_t num_params,
							 uint8_t brightness, uint64_t time_ms,
							 color_t *pixels, uint16_t num_pixels) {
	hsv_t color = {.h = params[0].value, .s = params[1].value, .v = brightness};

	for (uint16_t i = 0; i < num_pixels; i++) {
		pixels[i].hsv = color;
	}
}

/* --- Effect: Candle Table --- */

// Tabela de brilho gamma-corrected para efeito de chama (valores
// pré-calculados)
#include "table.h" // Tabela com valores de brilho

// Parâmetros do efeito vela table
static effect_param_t params_candle[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 50,
	 .min_value = 10,
	 .max_value = 100,
	 .step = 1},
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 16,
	 .min_value = 5,
	 .max_value = 30,
	 .step = 1},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 250,
	 .min_value = 0,
	 .max_value = 255,
	 .step = 5},
	{.name = "Segments",
	 .type = PARAM_TYPE_VALUE,
	 .value = 4,
	 .min_value = 1,
	 .max_value = 10,
	 .step = 1}, // Novo parâmetro
};

static void run_candle(const effect_param_t *params, uint8_t num_params,
					   uint8_t brightness, uint64_t time_ms, color_t *pixels,
					   uint16_t num_pixels) {
	uint16_t hue = params[1].value;
	uint8_t saturation = params[2].value;
	uint8_t speed = params[0].value;
	uint8_t num_segments = params[3].value;

	// Calcula LEDs por segmento (arredondando para baixo)
	uint16_t leds_per_segment = num_pixels / num_segments;

	for (uint8_t seg = 0; seg < num_segments; seg++) {
		// Offset único por segmento para variação independente
		uint32_t segment_offset = (seg * 7919) % CANDLE_TABLE_SIZE;

		// Índice na tabela de brilho (baseado no tempo + offset do segmento)
		uint32_t time_scaled = (time_ms * speed) / 50;
		uint32_t index = (time_scaled + segment_offset) % CANDLE_TABLE_SIZE;

		// Valor de brilho e cor ÚNICO para todo o segmento
		uint8_t value = CANDLE_TABLE_GAMMA[index];
		hsv_t hsv = {.h = hue, .s = saturation, .v = value};

		// Aplica a mesma cor/brilho a todos os LEDs do segmento
		uint16_t start_led = seg * leds_per_segment;
		uint16_t end_led = (seg + 1) * leds_per_segment;
		if (seg == num_segments - 1)
			end_led = num_pixels; // Ajuste para o último segmento

		for (uint16_t i = start_led; i < end_led; i++) {
			pixels[i].hsv = hsv;
		}
	}
}

/* --- Effect: Rainbow --- */

static effect_param_t params_rainbow[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 10,
	 .min_value = 10,
	 .max_value = 100,
	 .step = 1},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 100,
	 .min_value = 0,
	 .max_value = 100,
	 .step = 5},
};

static void run_rainbow(const effect_param_t *params, uint8_t num_params,
						uint8_t brightness, uint64_t time_ms, color_t *pixels,
						uint16_t num_pixels) {
	uint8_t speed = params[0].value;
	uint8_t saturation = params[1].value;

	for (uint16_t i = 0; i < num_pixels; i++) {
		uint32_t hue = ((time_ms * speed / 10) + (i * 360 / num_pixels)) % 360;
		pixels[i].hsv =
			(hsv_t){.h = (uint16_t)hue, .s = saturation, .v = brightness};
	}
}

/* --- Effect: Breathing --- */

static effect_param_t params_breathing[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 100,
	 .min_value = 10,
	 .max_value = 1000,
	 .step = 1},
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 180,
	 .min_value = 0,
	 .max_value = 359,
	 .step = 1},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 100,
	 .min_value = 0,
	 .max_value = 254,
	 .step = 5},
};

static void run_breathing(const effect_param_t *params, uint8_t num_params,
						  uint8_t brightness, uint64_t time_ms, color_t *pixels,
						  uint16_t num_pixels) {
	float speed = (float)params[0].value / 20.0f;
	uint16_t hue = params[1].value;
	uint8_t saturation = params[2].value;

	// Calculate brightness using a sine wave for a smooth breathing effect
	// The wave oscillates between 0 and 1.
	float wave = (sinf(time_ms * speed / 1000.0f) + 1.0f) / 2.0f;

	// Scale brightness from 0 to 100 for the HSV value
	uint8_t hsv_v = (uint8_t)(wave * 100.0f);

	hsv_t hsv = {.h = hue, .s = saturation, .v = hsv_v};

	for (uint16_t i = 0; i < num_pixels; i++) {
		pixels[i].hsv = hsv;
	}
}

/* --- List of all effects --- */
effect_t effect_candle = {.name = "Candle",
						  .run = run_candle,
						  .params = params_candle,
						  .num_params =
							  sizeof(params_candle) / sizeof(effect_param_t),
						  .pixel_format = PIXEL_FORMAT_HSV};

effect_t effect_breathing = {.name = "Breathing",
							 .run = run_breathing,
							 .params = params_breathing,
							 .num_params = sizeof(params_breathing) /
										   sizeof(effect_param_t),
							 .pixel_format = PIXEL_FORMAT_HSV};

effect_t effect_static_color = {.name = "Static Color",
								.run = run_static_color,
								.params = params_static_color,
								.num_params = sizeof(params_static_color) /
											  sizeof(effect_param_t),
								.pixel_format = PIXEL_FORMAT_HSV};

effect_t effect_rainbow = {.name = "Rainbow",
						   .run = run_rainbow,
						   .params = params_rainbow,
						   .num_params =
							   sizeof(params_rainbow) / sizeof(effect_param_t),
						   .pixel_format = PIXEL_FORMAT_HSV};

effect_t *effects[] = {&effect_candle, &effect_breathing, &effect_static_color,
					   &effect_rainbow};

const uint8_t effects_count = sizeof(effects) / sizeof(effects[0]);
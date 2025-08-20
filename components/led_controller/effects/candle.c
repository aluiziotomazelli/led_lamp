#include "led_effects.h" // For color_t, effect_param_t, etc.
#include "table.h"       // For CANDLE_TABLE
#include <stdint.h>

effect_param_t params_candle[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 1,
	 .min_value = 1,
	 .max_value = 50,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 1},
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 35,
	 .min_value = 5,
	 .max_value = 80,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 35},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 255,
	 .min_value = 0,
	 .max_value = 255,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 255},
	{.name = "Segments",
	 .type = PARAM_TYPE_VALUE,
	 .value = 4,
	 .min_value = 1,
	 .max_value = 10,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 4},
};

void run_candle(const effect_param_t *params, uint8_t num_params,
					   uint8_t brightness, uint64_t time_ms, color_t *pixels,
					   uint16_t num_pixels) {
	uint8_t speed = params[0].value;
	uint16_t hue = params[1].value;
	uint8_t saturation = params[2].value;
	uint8_t num_segments = params[3].value;

	// Parâmetros de variação (você pode ajustar esses valores conforme
	// necessário)
	uint8_t max_hue_variation = 15; // Variação máxima de matiz (0-255)
	uint8_t max_sat_variation = 15; // Variação máxima de saturação (0-255)
	uint8_t variation_speed = 1;	// Velocidade da variação (1-10)

	if (num_segments == 0)
		num_segments = 1;

	uint16_t leds_per_segment = num_pixels / num_segments;
	if (leds_per_segment == 0)
		leds_per_segment = 1;

	for (uint16_t seg = 0; seg < num_segments; seg++) {
		// Use segment index as a random-like offset. A prime number helps
		// decorrelate.
		uint32_t time_offset = seg * 877;
		uint32_t table_index =
			((time_ms * speed / 10) + time_offset) % CANDLE_TABLE_SIZE;
		uint32_t variation_index =
			((time_ms * variation_speed) + time_offset) % CANDLE_TABLE_SIZE;

		uint8_t v_from_table = CANDLE_TABLE[table_index];

		// Calcula variações baseadas na tabela para manter consistência
		int16_t hue_variation =
			((int16_t)CANDLE_TABLE[variation_index % CANDLE_TABLE_SIZE] - 128) *
			max_hue_variation / 128;
		int16_t sat_variation =
			((int16_t)CANDLE_TABLE[(variation_index + 67) % CANDLE_TABLE_SIZE] -
			 128) *
			max_sat_variation / 128;

		// Aplica as variações, garantindo que permaneçam dentro dos limites
		uint16_t varied_hue = (hue + hue_variation) % 360;
		// Aplica variação de saturação (com clamping manual 0-255)
		int16_t new_sat = saturation + sat_variation;
		if (new_sat < 0)
			new_sat = 0;
		if (new_sat > 255)
			new_sat = 255;
		uint8_t varied_sat = (uint8_t)new_sat;

		//		hsv_t hsv = {.h = hue, .s = saturation, .v = v_from_table};
		hsv_t hsv = {.h = varied_hue, .s = varied_sat, .v = v_from_table};

		uint16_t start_led = seg * leds_per_segment;
		uint16_t end_led = (seg + 1) * leds_per_segment;
		if (seg == num_segments - 1) {
			end_led = num_pixels; // Ensure last segment goes to the end
		}

		for (uint16_t i = start_led; i < end_led; i++) {
			pixels[i].hsv = hsv;
		}
	}
}

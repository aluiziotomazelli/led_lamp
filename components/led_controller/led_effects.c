#include "led_effects.h"
#include "candle_math.h"
#include "table.h"
#include <math.h>
#include <stdlib.h>

#include "hsv2rgb.h"

/* --- Effect: White Temp --- */

static effect_param_t params_white_temp[] = {
	{.name = "Temperature",
	 .type = PARAM_TYPE_VALUE,
	 .value = 0,
	 .min_value = 0,
	 .max_value = 5,
	 .step = 1},
};

static void run_white_temp(const effect_param_t *params, uint8_t num_params,
							 uint8_t brightness, uint64_t time_ms,
							 color_t *pixels, uint16_t num_pixels) {

	int16_t temp_index = params[0].value;
	rgb_t rgb;

	switch(temp_index){
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


/* --- Effect: Static Color --- */

static effect_param_t params_static_color[] = {
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 250,
	 .min_value = 0,
	 .max_value = 359,
	 .step = 1},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 0,
	 .min_value = 0,
	 .max_value = 255,
	 .step = 5},
};

static void run_static_color(const effect_param_t *params, uint8_t num_params,
							 uint8_t brightness, uint64_t time_ms,
							 color_t *pixels, uint16_t num_pixels) {
	hsv_t color = {
		.h = params[0].value,
		.s = params[1].value,
		.v = 255 // Full brightness, master brightness is applied in controller
	};
	//        ESP_LOGI("Cor estatica", "HSV, H = %d, S = %d, V = %d", color.h,
	//        color.s, color.v);

	rgb_t rgb;
	hsv_to_rgb_spectrum_deg(color.h, color.s, color.v, &rgb.r, &rgb.g, &rgb.b);

	for (uint16_t i = 0; i < num_pixels; i++) {
		pixels[i].rgb = rgb;
	}
}

/* --- Effect: Rainbow --- */

static effect_param_t params_rainbow[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 10,
	 .min_value = 1,
	 .max_value = 100,
	 .step = 1},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 255,
	 .min_value = 0,
	 .max_value = 255,
	 .step = 5},
};

static void run_rainbow(const effect_param_t *params, uint8_t num_params,
						uint8_t brightness, uint64_t time_ms, color_t *pixels,
						uint16_t num_pixels) {
	uint8_t speed = params[0].value;
	uint8_t saturation = params[1].value;

	for (uint16_t i = 0; i < num_pixels; i++) {
		uint32_t hue = ((time_ms * speed / 10) + (i * 360 / num_pixels)) % 360;
		pixels[i].hsv.h = (uint16_t)hue;
		pixels[i].hsv.s = saturation;
		pixels[i].hsv.v =
			255; // Full brightness, master brightness is applied in controller
	}
}

/* --- Effect: Breathing --- */

static effect_param_t params_breathing[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 5,
	 .min_value = 1,
	 .max_value = 100,
	 .step = 1},
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 200,
	 .min_value = 0,
	 .max_value = 359,
	 .step = 1},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 255,
	 .min_value = 0,
	 .max_value = 255,
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

	// Scale brightness from 0 to 255 for the HSV value
	uint8_t hsv_v = (uint8_t)(wave * 255.0f);

	hsv_t hsv = {.h = hue, .s = saturation, .v = hsv_v};

	for (uint16_t i = 0; i < num_pixels; i++) {
		pixels[i].hsv = hsv;
	}
}

/* --- Effect: Candle --- */

static effect_param_t params_candle[] = {
	{.name = "Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 1,
	 .min_value = 1,
	 .max_value = 50,
	 .step = 1},
	{.name = "Hue",
	 .type = PARAM_TYPE_HUE,
	 .value = 35,
	 .min_value = 5,
	 .max_value = 80,
	 .step = 1},
	{.name = "Saturation",
	 .type = PARAM_TYPE_SATURATION,
	 .value = 255,
	 .min_value = 0,
	 .max_value = 255,
	 .step = 1},
	{.name = "Segments",
	 .type = PARAM_TYPE_VALUE,
	 .value = 4,
	 .min_value = 1,
	 .max_value = 10,
	 .step = 1},
};

static void run_candle(const effect_param_t *params, uint8_t num_params,
					   uint8_t brightness, uint64_t time_ms, color_t *pixels,
					   uint16_t num_pixels) {
	uint8_t speed = params[0].value;
	uint16_t hue = params[1].value;
	uint8_t saturation = params[2].value;
	uint8_t num_segments = params[3].value;

	// Parâmetros de variação (você pode ajustar esses valores conforme
	// necessário)
	uint8_t max_hue_variation = 15;	// Variação máxima de matiz (0-255)
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
        if (new_sat < 0) new_sat = 0;
        if (new_sat > 255) new_sat = 255;
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

/* --- Effect: Candle Math --- */

static effect_param_t params_candle_math[] = {
    {.name = "Speed", .type = PARAM_TYPE_SPEED, .value = 20, .min_value = 1, .max_value = 100, .step = 1},
    {.name = "Hue", .type = PARAM_TYPE_HUE, .value = 30, .min_value = 0, .max_value = 359, .step = 1},
    {.name = "Saturation", .type = PARAM_TYPE_SATURATION, .value = 255, .min_value = 0, .max_value = 255, .step = 5},
    {.name = "Segments", .type = PARAM_TYPE_VALUE, .value = 10, .min_value = 1, .max_value = NUM_LEDS, .step = 1},
    {.name = "Intensity", .type = PARAM_TYPE_VALUE, .value = 30, .min_value = 0, .max_value = 100, .step = 5},
    {.name = "Dip Prob", .type = PARAM_TYPE_VALUE, .value = 2, .min_value = 0, .max_value = 100, .step = 1},
};

static void run_candle_math(const effect_param_t *params, uint8_t num_params,
                       uint8_t brightness, uint64_t time_ms, color_t *pixels,
                       uint16_t num_pixels) {

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

    if (p_segments == 0) p_segments = 1;
    if (p_segments > num_pixels) p_segments = num_pixels;


    // Re-initialize if number of pixels or zones changes
    if (candle_effect == NULL || num_pixels != last_num_pixels || p_segments != last_num_zones) {
        if (candle_effect != NULL) {
            candle_effect_deinit(candle_effect);
        }

        candle_config_t config = {
            .num_zones = p_segments,
            .leds_per_zone = (p_segments > 0) ? (num_pixels / p_segments) : num_pixels,
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
        last_time_ms = time_ms; // Reset time to avoid large delta on first frame
    }

    // Update config with current parameters
    candle_effect->config.flicker_speed = (float)p_speed / 20.0f;
    candle_effect->config.base_hue = p_hue;
    candle_effect->config.base_sat = p_sat;
    candle_effect->config.flicker_intensity = (float)p_intensity / 100.0f;
    candle_effect->config.dip_probability = (float)p_dip_prob / 1000.0f;
    candle_effect->config.leds_per_zone = (p_segments > 0) ? (num_pixels / p_segments) : num_pixels;


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
        pixels[i].hsv.v = (uint8_t)((float)original_v * master_brightness_multiplier);
    }
}

/* --- List of all effects --- */
effect_t effect_candle = {.name = "Candle",
						  .run = run_candle,
						  .color_mode = COLOR_MODE_HSV,
						  .params = params_candle,
						  .num_params =
							  sizeof(params_candle) / sizeof(effect_param_t),
						  .is_dynamic = true};

effect_t effect_white_temp = {.name = "White Temp",
						  .run = run_white_temp,
						  .color_mode = COLOR_MODE_RGB,
						  .params = params_white_temp,
						  .num_params =
							  sizeof(params_white_temp) / sizeof(effect_param_t),
						  .is_dynamic = false};

effect_t effect_breathing = {.name = "Breathing",
							 .run = run_breathing,
							 .color_mode = COLOR_MODE_HSV,
							 .params = params_breathing,
							 .num_params = sizeof(params_breathing) /
										   sizeof(effect_param_t),
							 .is_dynamic = true};

effect_t effect_static_color = {.name = "Static Color",
								.run = run_static_color,
								.color_mode = COLOR_MODE_RGB,
								.params = params_static_color,
								.num_params = sizeof(params_static_color) /
											  sizeof(effect_param_t),
								.is_dynamic = false};

effect_t effect_rainbow = {.name = "Rainbow",
						   .run = run_rainbow,
						   .color_mode = COLOR_MODE_HSV,
						   .params = params_rainbow,
						   .num_params =
							   sizeof(params_rainbow) / sizeof(effect_param_t),
						   .is_dynamic = true};

effect_t effect_candle_math = {.name = "Candle Math",
                           .run = run_candle_math,
                           .color_mode = COLOR_MODE_HSV,
                           .params = params_candle_math,
                           .num_params =
                               sizeof(params_candle_math) / sizeof(effect_param_t),
                           .is_dynamic = true};

effect_t *effects[] = {&effect_white_temp, &effect_candle, &effect_candle_math, &effect_static_color, &effect_breathing,
					   &effect_rainbow};

const uint8_t effects_count = sizeof(effects) / sizeof(effects[0]);
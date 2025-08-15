#include "led_effects.h"
#include "table.h"
#include <math.h> // For fmod in hsv_to_rgb
#include <stdlib.h>

// Helper function to convert HSV to RGB
// Source: https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-c
static void hsv_to_rgb(hsv_t hsv, rgb_t *rgb) {
    if (hsv.s == 0) {
        // achromatic (grey)
        uint8_t val = (hsv.v * 255) / 100;
        rgb->r = rgb->g = rgb->b = val;
        return;
    }

    float h = fmod(hsv.h, 360.0f);
    float s = (float)hsv.s / 100.0f;
    float v = (float)hsv.v / 100.0f;

    int i = floor(h / 60.0f);
    float f = (h / 60.0f) - i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    float r, g, b;

    switch (i % 6) {
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        default: r = v, g = p, b = q; break;
    }

    rgb->r = r * 255;
    rgb->g = g * 255;
    rgb->b = b * 255;
}


/* --- Effect: Static Color --- */

static effect_param_t params_static_color[] = {
    { .name = "Hue", .type = PARAM_TYPE_HUE, .value = 0, .min_value = 0, .max_value = 359, .step = 1 },
    { .name = "Saturation", .type = PARAM_TYPE_SATURATION, .value = 100, .min_value = 0, .max_value = 100, .step = 5 },
};

static void run_static_color(const effect_param_t *params, uint8_t num_params, uint8_t brightness, uint64_t time_ms, color_t *pixels, uint16_t num_pixels) {
    hsv_t color = {
        .h = params[0].value,
        .s = params[1].value,
        .v = 100 // We'll apply master brightness in the controller
    };

    for (uint16_t i = 0; i < num_pixels; i++) {
        pixels[i].hsv = color;
    }
}


/* --- Effect: Rainbow --- */

static effect_param_t params_rainbow[] = {
    { .name = "Speed", .type = PARAM_TYPE_SPEED, .value = 10, .min_value = 1, .max_value = 100, .step = 1 },
    { .name = "Saturation", .type = PARAM_TYPE_SATURATION, .value = 100, .min_value = 0, .max_value = 100, .step = 5 },
};

static void run_rainbow(const effect_param_t *params, uint8_t num_params, uint8_t brightness, uint64_t time_ms, color_t *pixels, uint16_t num_pixels) {
    uint8_t speed = params[0].value;
    uint8_t saturation = params[1].value;

    for (uint16_t i = 0; i < num_pixels; i++) {
        uint32_t hue = ((time_ms * speed / 10) + (i * 360 / num_pixels)) % 360;
        pixels[i].hsv.h = (uint16_t)hue;
        pixels[i].hsv.s = saturation;
        pixels[i].hsv.v = 100; // We'll apply master brightness in the controller
    }
}


/* --- Effect: Breathing --- */

static effect_param_t params_breathing[] = {
    { .name = "Speed", .type = PARAM_TYPE_SPEED, .value = 5, .min_value = 1, .max_value = 100, .step = 1 },
    { .name = "Hue", .type = PARAM_TYPE_HUE, .value = 200, .min_value = 0, .max_value = 359, .step = 1 },
    { .name = "Saturation", .type = PARAM_TYPE_SATURATION, .value = 100, .min_value = 0, .max_value = 100, .step = 5 },
};

static void run_breathing(const effect_param_t *params, uint8_t num_params, uint8_t brightness, uint64_t time_ms, color_t *pixels, uint16_t num_pixels) {
    float speed = (float)params[0].value / 20.0f;
    uint16_t hue = params[1].value;
    uint8_t saturation = params[2].value;

    // Calculate brightness using a sine wave for a smooth breathing effect
    // The wave oscillates between 0 and 1.
    float wave = (sinf(time_ms * speed / 1000.0f) + 1.0f) / 2.0f;

    // Scale brightness from 0 to 100 for the HSV value
    uint8_t hsv_v = (uint8_t)(wave * 100.0f);

    hsv_t hsv = { .h = hue, .s = saturation, .v = hsv_v };

    for (uint16_t i = 0; i < num_pixels; i++) {
        pixels[i].hsv = hsv;
    }
}


/* --- Effect: Candle --- */

static effect_param_t params_candle[] = {
    {.name = "Speed", .type = PARAM_TYPE_SPEED, .value = 50, .min_value = 10, .max_value = 100, .step = 1},
    {.name = "Hue", .type = PARAM_TYPE_HUE, .value = 16, .min_value = 5, .max_value = 30, .step = 1},
    {.name = "Saturation", .type = PARAM_TYPE_SATURATION, .value = 250, .min_value = 200, .max_value = 254, .step = 1},
    {.name = "Segments", .type = PARAM_TYPE_VALUE, .value = 4, .min_value = 1, .max_value = 10, .step = 1},
};

static void run_candle(const effect_param_t *params, uint8_t num_params, uint8_t brightness, uint64_t time_ms, color_t *pixels, uint16_t num_pixels) {
    uint8_t speed = params[0].value;
    uint16_t hue = params[1].value;
    uint8_t saturation = params[2].value;
    uint8_t num_segments = params[3].value;

    if (num_segments == 0) num_segments = 1;

    uint16_t leds_per_segment = num_pixels / num_segments;
    if (leds_per_segment == 0) leds_per_segment = 1;

    for (uint16_t seg = 0; seg < num_segments; seg++) {
        // Use segment index as a random-like offset. A prime number helps decorrelate.
        uint32_t time_offset = seg * 131;
        uint32_t table_index = ((time_ms * speed / 10) + time_offset) % CANDLE_TABLE_SIZE;

        uint8_t v_from_table = CANDLE_TABLE[table_index];

        // Scale the brightness value from the table (0-255) to the HSV V range (0-100)
        uint8_t hsv_v = (v_from_table * 100) / 255;

        hsv_t hsv = {
            .h = hue,
            .s = saturation, // Using unscaled value as requested by user
            .v = hsv_v
        };

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


/* --- List of all effects --- */

effect_t effect_candle = {
    .name = "Candle",
    .run = run_candle,
    .color_mode = COLOR_MODE_HSV,
    .params = params_candle,
    .num_params = sizeof(params_candle) / sizeof(effect_param_t)
};

effect_t effect_breathing = {
    .name = "Breathing",
    .run = run_breathing,
    .color_mode = COLOR_MODE_HSV,
    .params = params_breathing,
    .num_params = sizeof(params_breathing) / sizeof(effect_param_t)
};

effect_t effect_static_color = {
    .name = "Static Color",
    .run = run_static_color,
    .color_mode = COLOR_MODE_HSV,
    .params = params_static_color,
    .num_params = sizeof(params_static_color) / sizeof(effect_param_t)
};

effect_t effect_rainbow = {
    .name = "Rainbow",
    .run = run_rainbow,
    .color_mode = COLOR_MODE_HSV,
    .params = params_rainbow,
    .num_params = sizeof(params_rainbow) / sizeof(effect_param_t)
};

effect_t *effects[] = {
    &effect_candle,
    &effect_breathing,
    &effect_static_color,
    &effect_rainbow
};

const uint8_t effects_count = sizeof(effects) / sizeof(effects[0]);
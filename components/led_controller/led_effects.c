#include "led_effects.h"
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

static void run_static_color(const effect_param_t *params, uint8_t num_params, uint8_t brightness, uint64_t time_ms, rgb_t *pixels, uint16_t num_pixels) {
    hsv_t color = {
        .h = params[0].value,
        .s = params[1].value,
        .v = 100
    };

    rgb_t rgb_color;
    hsv_to_rgb(color, &rgb_color);

    for (uint16_t i = 0; i < num_pixels; i++) {
        pixels[i] = rgb_color;
    }
}


/* --- Effect: Rainbow --- */

static effect_param_t params_rainbow[] = {
    { .name = "Speed", .type = PARAM_TYPE_SPEED, .value = 10, .min_value = 1, .max_value = 100, .step = 1 },
    { .name = "Saturation", .type = PARAM_TYPE_SATURATION, .value = 100, .min_value = 0, .max_value = 100, .step = 5 },
};

static void run_rainbow(const effect_param_t *params, uint8_t num_params, uint8_t brightness, uint64_t time_ms, rgb_t *pixels, uint16_t num_pixels) {
    uint8_t speed = params[0].value;
    uint8_t saturation = params[1].value;

    for (uint16_t i = 0; i < num_pixels; i++) {
        uint32_t hue = ((time_ms * speed / 10) + (i * 360 / num_pixels)) % 360;
        hsv_t hsv = { .h = (uint16_t)hue, .s = saturation, .v = 100 };
        hsv_to_rgb(hsv, &pixels[i]);
    }
}


/* --- Effect: Breathing --- */

static effect_param_t params_breathing[] = {
    { .name = "Speed", .type = PARAM_TYPE_SPEED, .value = 5, .min_value = 1, .max_value = 100, .step = 1 },
    { .name = "Hue", .type = PARAM_TYPE_HUE, .value = 200, .min_value = 0, .max_value = 359, .step = 1 },
    { .name = "Saturation", .type = PARAM_TYPE_SATURATION, .value = 100, .min_value = 0, .max_value = 100, .step = 5 },
};

static void run_breathing(const effect_param_t *params, uint8_t num_params, uint8_t brightness, uint64_t time_ms, rgb_t *pixels, uint16_t num_pixels) {
    float speed = (float)params[0].value / 20.0f;
    uint16_t hue = params[1].value;
    uint8_t saturation = params[2].value;

    // Calculate brightness using a sine wave for a smooth breathing effect
    // The wave oscillates between 0 and 1.
    float wave = (sinf(time_ms * speed / 1000.0f) + 1.0f) / 2.0f;

    // Scale brightness from 0 to 100 for the HSV value
    uint8_t hsv_v = (uint8_t)(wave * 100.0f);

    hsv_t hsv = { .h = hue, .s = saturation, .v = hsv_v };
    rgb_t rgb_color;
    hsv_to_rgb(hsv, &rgb_color);

    for (uint16_t i = 0; i < num_pixels; i++) {
        pixels[i] = rgb_color;
    }
}


/* --- List of all effects --- */

effect_t effect_breathing = {
    .name = "Breathing",
    .run = run_breathing,
    .params = params_breathing,
    .num_params = sizeof(params_breathing) / sizeof(effect_param_t)
};

effect_t effect_static_color = {
    .name = "Static Color",
    .run = run_static_color,
    .params = params_static_color,
    .num_params = sizeof(params_static_color) / sizeof(effect_param_t)
};

effect_t effect_rainbow = {
    .name = "Rainbow",
    .run = run_rainbow,
    .params = params_rainbow,
    .num_params = sizeof(params_rainbow) / sizeof(effect_param_t)
};

effect_t *effects[] = {
    &effect_breathing,
    &effect_static_color,
    &effect_rainbow
};

const uint8_t effects_count = sizeof(effects) / sizeof(effects[0]);
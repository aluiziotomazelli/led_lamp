#pragma once
#include <stdint.h>
#include "led_effects.h" // For color_t

typedef struct {
    // Configuration
    uint16_t num_zones;
    uint16_t leds_per_zone;
    float flicker_speed;      // 0.05f
    float dip_probability;    // 0.01f
    float recovery_rate;     // 0.05f
    float min_brightness;    // 20.0f
    float max_brightness;    // 100.0f
    float base_brightness;  // 70.0f
    float flicker_intensity; // 0.2f


    // Color config
    uint16_t base_hue;
    uint16_t min_hue;
    uint16_t max_hue;
    uint16_t base_sat;
    uint16_t min_sat;
    uint16_t max_sat;
} candle_config_t;

typedef struct {
    candle_config_t config;
    float* zone_brightness;
    float global_brightness;
} candle_effect_t;

// Initialize effect
candle_effect_t* candle_effect_init(const candle_config_t* config);

// Update effect
void candle_effect_update(candle_effect_t* effect, float delta_time, color_t *pixels, uint16_t num_pixels);

// Cleanup
void candle_effect_deinit(candle_effect_t* effect);

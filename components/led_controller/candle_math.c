/**
 * @file candle_math.c
 * @brief Candle Effect Mathematical Implementation
 * 
 * @details This file implements the realistic candle flame simulation using
 *          mathematical models for flickering, brightness variation, and
 *          zone-based control. Includes noise generation, dip simulation,
 *          and smooth interpolation for natural flame behavior.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#include "candle_math.h"
#include <stdlib.h>
#include <math.h>

//------------------------------------------------------------------------------
// PRIVATE FUNCTION DECLARATIONS
//------------------------------------------------------------------------------

/**
 * @brief Generates pseudo-random noise for natural flickering effects
 * 
 * @param x First noise input (typically time-based)
 * @param y Second noise input (typically position-based)
 * @return Float between 0.0 and 1.0
 */
static float flicker_noise(uint32_t x, uint32_t y);

/**
 * @brief Applies random brightness dips to simulate flame instability
 * 
 * @param current Current brightness value
 * @param zone_id Zone index where dip is being applied
 * @param time Current time value for noise generation
 * @param dip_prob Probability of a dip occurring (0.0-1.0)
 * @return New brightness value after potential dip
 */
static float apply_dips(float current, int zone_id, float time, float dip_prob);

//------------------------------------------------------------------------------
// PRIVATE FUNCTION IMPLEMENTATIONS
//------------------------------------------------------------------------------

/**
 * @brief Generates pseudo-random noise for natural flickering effects
 */
static float flicker_noise(uint32_t x, uint32_t y) {
    // XOR and multiply operations to create pseudo-random values
    x = (x >> 13) ^ x;
    x = (x * (x * x * 60493 + 19990303) + 1376312589) & 0x7fffffff;
    y = (y >> 13) ^ y;
    y = (y * (y * y * 60493 + 19990303) + 1376312589) & 0x7fffffff;
    return (float)((x + y) & 0x7fffffff) / 2147483647.0f;
}

/**
 * @brief Applies random brightness dips to simulate flame instability
 */
static float apply_dips(float current, int zone_id, float time, float dip_prob) {
    if ((float)rand() / RAND_MAX < dip_prob) {
        float severity = 0.3f + 0.5f * flicker_noise((uint32_t)time, zone_id);
        float new_val = current * severity;
        // ESP_LOGD(TAG, "Zone %d dip: %.1f -> %.1f", zone_id, current, new_val);
        return new_val;
    }
    return current;
}

//------------------------------------------------------------------------------
// PUBLIC FUNCTION IMPLEMENTATIONS
//------------------------------------------------------------------------------

/**
 * @brief Initialize a new candle effect instance
 */
candle_effect_t* candle_effect_init(const candle_config_t* config) {
    candle_effect_t* effect = calloc(1, sizeof(candle_effect_t));
    if (!effect) {
        return NULL;
    }
    
    effect->config = *config;
    effect->global_brightness = 1.0f;

    // Allocate zone brightness array
    effect->zone_brightness = malloc(config->num_zones * sizeof(float));
    if (!effect->zone_brightness) {
        free(effect);
        return NULL;
    }

    // Initialize all zones to base brightness
    for (int z = 0; z < config->num_zones; z++) {
        effect->zone_brightness[z] = config->base_brightness;
    }

    return effect;
}

/**
 * @brief Update the candle effect simulation
 */
void candle_effect_update(candle_effect_t* effect, float delta_time, color_t *pixels, uint16_t num_pixels) {
    static float time = 0.0f;
    time += delta_time * effect->config.flicker_speed;

    // Update each zone's brightness with flicker and dips
    for (int z = 0; z < effect->config.num_zones; z++) {
        // Calculate target brightness with noise-based flicker
        float target = effect->config.base_brightness +
                     effect->config.flicker_intensity *
                     (effect->config.max_brightness - effect->config.min_brightness) *
                     (flicker_noise((uint32_t)(time * 1000), z * 100) - 0.5f);

        // Apply random dips to simulate flame instability
        effect->zone_brightness[z] = apply_dips(
            effect->zone_brightness[z],
            z,
            time,
            effect->config.dip_probability
        );

        // Smoothly interpolate towards target brightness
        effect->zone_brightness[z] += (target - effect->zone_brightness[z]) *
                                    effect->config.recovery_rate;

        // Clamp brightness to valid range
        effect->zone_brightness[z] = fmaxf(effect->config.min_brightness,
                                         fminf(effect->config.max_brightness,
                                               effect->zone_brightness[z]));
    }

    // Update LED buffer with calculated brightness values
    for (int z = 0; z < effect->config.num_zones; z++) {
        for (int i = 0; i < effect->config.leds_per_zone; i++) {
            int led_idx = z * effect->config.leds_per_zone + i;
            if (led_idx >= num_pixels) continue;

            // Convert brightness percentage to HSV value (0-255)
            float brightness_percent = effect->zone_brightness[z] * effect->global_brightness;
            uint8_t hsv_v = (uint8_t)(brightness_percent / 100.0f * 255.0f);

            // Set HSV color for this pixel
            pixels[led_idx].hsv.h = effect->config.base_hue;
            pixels[led_idx].hsv.s = effect->config.base_sat;
            pixels[led_idx].hsv.v = hsv_v;
        }
    }
}

/**
 * @brief Deinitialize and cleanup candle effect instance
 */
void candle_effect_deinit(candle_effect_t* effect) {
    if (effect) {
        free(effect->zone_brightness);
        free(effect);
    }
}
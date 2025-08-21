/**
 * @file candle_math_logic.c
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

// Standard library includes
#include <stdlib.h>
#include <math.h>

// Project specific headers
#include "candle_math_logic.h"

//------------------------------------------------------------------------------
// PRIVATE FUNCTION DECLARATIONS
//------------------------------------------------------------------------------

/**
 * @brief Generates pseudo-random noise for natural flickering effects
 * 
 * @param x First noise input (typically time-based)
 * @param y Second noise input (typically position-based)
 * @return Float between 0.0 and 1.0
 * 
 * @note Uses XOR and multiply operations for deterministic pseudo-randomness
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
 * 
 * @note Uses probability check and severity calculation based on noise
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
    // This provides deterministic randomness suitable for animation
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
    // Check if a dip should occur based on probability
    if ((float)rand() / RAND_MAX < dip_prob) {
        // Calculate dip severity using noise for natural variation
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
 * 
 * @param[in] config Pointer to candle configuration structure
 * @return candle_effect_t* Pointer to initialized effect instance, NULL on failure
 * 
 * @note Allocates memory for effect structure and zone brightness array
 * @warning Returns NULL if memory allocation fails
 */
candle_effect_t* candle_effect_init(const candle_config_t* config) {
    candle_effect_t* effect = calloc(1, sizeof(candle_effect_t));
    if (!effect) {
        return NULL;
    }
    
    // Copy configuration and initialize default values
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
 * 
 * @param[in] effect Pointer to candle effect instance
 * @param[in] delta_time Time elapsed since last update (in seconds)
 * @param[out] pixels Output pixel buffer for rendered colors
 * @param[in] num_pixels Number of pixels in the output buffer
 * 
 * @note Updates zone brightness with flicker, applies dips, and renders to pixels
 * @warning Ensure pixel buffer has sufficient capacity for num_pixels
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
 * 
 * @param[in] effect Pointer to candle effect instance to cleanup
 * 
 * @note Releases all allocated memory including zone brightness buffers
 * @warning Safe to call with NULL pointer (no operation performed)
 */
void candle_effect_deinit(candle_effect_t* effect) {
    if (effect) {
        free(effect->zone_brightness);
        free(effect);
    }
}
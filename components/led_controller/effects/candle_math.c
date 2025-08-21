/**
 * @file candle_math.c
 * @brief Mathematical candle flame LED effect implementation
 * 
 * @details Implements a physically-based candle flame simulation using
 *          mathematical models and state machines for realistic flickering
 *          with dynamic intensity variations and dip behaviors
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// Project specific headers
#include "candle_math_logic.h"
#include "led_effects.h"

// Standard library includes
#include <math.h>
#include <stdlib.h>

/**
 * @brief Runs the mathematical candle effect algorithm
 * 
 * @param[in] params      Array of effect parameters
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Maintains internal state for consistent flame behavior across frames
 *       and handles dynamic reconfiguration when parameters change
 * 
 * @warning The candle_effect state is static and persists between calls
 * @warning Ensure proper initialization of candle_math_logic components
 */
void run_candle_math(const effect_param_t *params, uint8_t num_params,
                     uint8_t brightness, uint64_t time_ms,
                     color_t *pixels, uint16_t num_pixels) {

    // Static variables for maintaining effect state across calls
    static candle_effect_t *candle_effect = NULL;
    static uint16_t last_num_pixels = 0;
    static uint16_t last_num_zones = 0;
    static uint64_t last_time_ms = 0;

    // Extract parameters from the parameter array
    uint8_t p_speed = params[0].value;
    uint16_t p_hue = params[1].value;
    uint8_t p_sat = params[2].value;
    uint8_t p_segments = params[3].value;
    uint8_t p_intensity = params[4].value;
    uint8_t p_dip_prob = params[5].value;

    // Validate and clamp segment parameter
    if (p_segments == 0)
        p_segments = 1;
    if (p_segments > num_pixels)
        p_segments = num_pixels;

    // Re-initialize if number of pixels or zones changes
    // This ensures the effect adapts to changing LED configurations
    if (candle_effect == NULL || num_pixels != last_num_pixels ||
        p_segments != last_num_zones) {
        if (candle_effect != NULL) {
            candle_effect_deinit(candle_effect);
        }

        // Configure candle effect with current parameters
        candle_config_t config = {
            .num_zones = p_segments,
            .leds_per_zone = (p_segments > 0) ? (num_pixels / p_segments) : num_pixels,
            .flicker_speed = 0.05f,
            .dip_probability = 0.02f,
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

    // Update configuration with current parameter values
    // Convert integer parameters to appropriate floating-point ranges
    candle_effect->config.flicker_speed = (float)p_speed / 20.0f;
    candle_effect->config.base_hue = p_hue;
    candle_effect->config.base_sat = p_sat;
    candle_effect->config.flicker_intensity = (float)p_intensity / 100.0f;
    candle_effect->config.dip_probability = (float)p_dip_prob / 1000.0f;
    candle_effect->config.leds_per_zone = (p_segments > 0) ? (num_pixels / p_segments) : num_pixels;

    // Calculate delta time for the physics simulation
    // Ensures consistent behavior regardless of frame rate
    float delta_time = 0.0f;
    if (time_ms > last_time_ms) {
        delta_time = (time_ms - last_time_ms) / 1000.0f;
    }
    last_time_ms = time_ms;

    // Run the candle simulation with current parameters and time delta
    candle_effect_update(candle_effect, delta_time, pixels, num_pixels);

    // Apply master brightness scaling to all pixels
    // This allows global brightness control without affecting the simulation
    float master_brightness_multiplier = (float)brightness / 255.0f;
    for (uint16_t i = 0; i < num_pixels; i++) {
        uint16_t original_v = pixels[i].hsv.v;
        pixels[i].hsv.v = (uint8_t)((float)original_v * master_brightness_multiplier);
    }
}
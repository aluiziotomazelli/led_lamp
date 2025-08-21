/**
 * @file candle.c
 * @brief Candle flame LED effect implementation
 * 
 * @details Implements a realistic candle flame simulation with flickering
 *          effects using precomputed noise tables for natural variations
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// Project specific headers
#include "led_effects.h" // For color_t, effect_param_t, etc.
#include "table.h"       // For CANDLE_TABLE

// Standard library includes
#include <stdint.h>

/**
 * @brief Runs the candle effect algorithm
 * 
 * @param[in] params      Array of effect parameters (speed, hue, saturation, segments)
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Uses precomputed noise tables to create realistic flickering patterns
 *       with variations in brightness, hue, and saturation across segments
 * 
 * @warning Ensure CANDLE_TABLE is properly initialized before calling this function
 */
void run_candle(const effect_param_t *params, uint8_t num_params,
                uint8_t brightness, uint64_t time_ms, color_t *pixels,
                uint16_t num_pixels) {
    // Extract effect parameters
    uint8_t speed = params[0].value;
    uint16_t hue = params[1].value;
    uint8_t saturation = params[2].value;
    uint8_t num_segments = params[3].value;

    // Variation parameters (these values can be adjusted as needed)
    uint8_t max_hue_variation = 15; // Maximum hue variation (0-255)
    uint8_t max_sat_variation = 15; // Maximum saturation variation (0-255)
    uint8_t variation_speed = 1;    // Variation speed (1-10)

    // Ensure at least one segment
    if (num_segments == 0)
        num_segments = 1;

    // Calculate LEDs per segment with bounds checking
    uint16_t leds_per_segment = num_pixels / num_segments;
    if (leds_per_segment == 0)
        leds_per_segment = 1;

    // Process each segment independently with unique variations
    for (uint16_t seg = 0; seg < num_segments; seg++) {
        // Use segment index as a random-like offset. A prime number helps
        // decorrelate segments for more natural appearance
        uint32_t time_offset = seg * 877;
        
        // Calculate indices into the candle flicker table for this segment
        uint32_t table_index = ((time_ms * speed / 10) + time_offset) % CANDLE_TABLE_SIZE;
        uint32_t variation_index = ((time_ms * variation_speed) + time_offset) % CANDLE_TABLE_SIZE;

        // Get brightness value from precomputed candle flicker table
        uint8_t v_from_table = CANDLE_TABLE[table_index];

        // Calculate hue and saturation variations based on noise table
        // to maintain temporal consistency while introducing randomness
        int16_t hue_variation = ((int16_t)CANDLE_TABLE[variation_index % CANDLE_TABLE_SIZE] - 128) * max_hue_variation / 128;
        int16_t sat_variation = ((int16_t)CANDLE_TABLE[(variation_index + 67) % CANDLE_TABLE_SIZE] - 128) * max_sat_variation / 128;

        // Apply variations with bounds checking
        uint16_t varied_hue = (hue + hue_variation) % 360;
        
        // Apply saturation variation with clamping to valid range (0-255)
        int16_t new_sat = saturation + sat_variation;
        if (new_sat < 0)
            new_sat = 0;
        if (new_sat > 255)
            new_sat = 255;
        uint8_t varied_sat = (uint8_t)new_sat;

        // Create HSV color with variations for this segment
        hsv_t hsv = {.h = varied_hue, .s = varied_sat, .v = v_from_table};

        // Calculate segment boundaries
        uint16_t start_led = seg * leds_per_segment;
        uint16_t end_led = (seg + 1) * leds_per_segment;
        
        // Ensure last segment covers all remaining pixels
        if (seg == num_segments - 1) {
            end_led = num_pixels;
        }

        // Apply the varied candle color to all pixels in this segment
        for (uint16_t i = start_led; i < end_led; i++) {
            pixels[i].hsv = hsv;
        }
    }
}
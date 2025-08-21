/**
 * @file breathing.c
 * @brief Breathing LED effect implementation
 * 
 * @details Implements a smooth pulsating breathing effect using sine wave
 *          modulation of brightness for a natural breathing appearance
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// Project specific headers
#include "led_effects.h"

// Standard library includes
#include <math.h>

/* --- Effect: Breathing --- */

/**
 * @brief Runs the breathing effect algorithm
 * 
 * @param[in] params      Array of effect parameters (speed, hue, saturation)
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Uses a sine wave to create smooth brightness transitions
 *       between minimum and maximum intensity for breathing effect
 * 
 * @warning Ensure num_params >= 3 to avoid parameter access violations
 */
void run_breathing(const effect_param_t *params, uint8_t num_params,
                   uint8_t brightness, uint64_t time_ms, color_t *pixels,
                   uint16_t num_pixels) {
    // Extract effect parameters
    float speed = (float)params[0].value / 20.0f;
    uint16_t hue = params[1].value;
    uint8_t saturation = params[2].value;

    // Calculate brightness using a sine wave for a smooth breathing effect
    // The wave oscillates between 0 and 1 with time-based modulation
    float wave = (sinf(time_ms * speed / 1000.0f) + 1.0f) / 2.0f;

    // Scale brightness from 0 to 255 for the HSV value
    // Apply master brightness scaling to the wave modulation
    uint8_t hsv_v = (uint8_t)(wave * 255.0f);

    // Create HSV color structure with modulated brightness
    hsv_t hsv = {.h = hue, .s = saturation, .v = hsv_v};

    // Apply the same breathing color to all pixels
    for (uint16_t i = 0; i < num_pixels; i++) {
        pixels[i].hsv = hsv;
    }
}
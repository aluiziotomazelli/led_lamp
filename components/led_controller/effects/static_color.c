/**
 * @file static_color.c
 * @brief Static Color LED effect implementation
 * 
 * @details Implements a simple static color effect that displays a uniform
 *          color across all LEDs with configurable hue and saturation
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// Project specific headers
#include "led_effects.h" // For color_t, effect_param_t, etc.

// Standard library includes
#include <stdint.h>

/**
 * @brief Runs the static color effect algorithm
 * 
 * @param[in] params      Array of effect parameters (hue, saturation)
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds (unused)
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Applies the same HSV color to all LEDs for a uniform static display
 *       Master brightness is passed through to allow external control
 * 
 * @warning This is a time-independent effect - animation timing is ignored
 * @note One of the simplest effects, useful for testing and solid color displays
 */
void run_static_color(const effect_param_t *params, uint8_t num_params,
                     uint8_t brightness, uint64_t time_ms,
                     color_t *pixels, uint16_t num_pixels) {
    // Create HSV color structure from parameters
    // Master brightness is applied directly as the value component
    hsv_t hsv = {
        .h = params[0].value,     // Hue from first parameter
        .s = params[1].value,     // Saturation from second parameter
        .v = brightness           // Full brightness controlled by master level
    };

    // Apply the same static color to all pixels in the buffer
    for (uint16_t i = 0; i < num_pixels; i++) {
        pixels[i].hsv = hsv;
    }
}
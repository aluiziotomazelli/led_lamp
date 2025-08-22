/**
 * @file white_temp.c
 * @brief White Temperature LED effect implementation
 * 
 * @details Implements a white color temperature effect with predefined
 *          RGB values representing different white temperatures from
 *          warm to cool color spectra
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
 * @brief Runs the white temperature effect algorithm
 * 
 * @param[in] params      Array of effect parameters (temperature index)
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds (unused)
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Uses predefined RGB values for different white temperature levels
 *       ranging from warm (reddish) to cool (bluish) white
 * 
 * @warning Temperature index must be between 0-5, defaults to neutral if out of range
 * @note Brightness is controlled externally by the LED controller
 */
void run_white_temp(const effect_param_t *params, uint8_t num_params,
                   uint8_t brightness, uint64_t time_ms,
                   color_t *pixels, uint16_t num_pixels) {

    // Get temperature index from parameters
    int16_t temp_index = params[0].value;
    rgb_t rgb;

    // Select RGB values based on temperature index
    // Predefined values represent different white color temperatures from warm to cool
    switch (temp_index) {
    case 0: // Deepest warm white (candlelight)
        rgb = (rgb_t){255, 120, 0};
        break;
    case 1: // Very warm white
        rgb = (rgb_t){255, 130, 30};
        break;
    case 2: // Warm white
        rgb = (rgb_t){255, 140, 50};
        break;
    case 3: // Warm-neutral white
        rgb = (rgb_t){255, 160, 80};
        break;
    case 4: // Neutral white
        rgb = (rgb_t){255, 197, 143};
        break;
    case 5: // Cool-neutral white
        rgb = (rgb_t){255, 214, 170};
        break;
    case 6: // Cool white
        rgb = (rgb_t){255, 240, 220};
        break;
    case 7: // Pure white
        rgb = (rgb_t){255, 255, 255};
        break;
    case 8: // Ice cold white
        rgb = (rgb_t){201, 226, 255};
        break;
    case 9: // Deep cool white (bluish)
        rgb = (rgb_t){180, 210, 255};
        break;
    default: // Fallback to neutral white if index is out of range
        rgb = (rgb_t){255, 197, 143};
        break;
    }

    // Apply the selected white temperature to all pixels
    for (uint16_t i = 0; i < num_pixels; i++) {
        pixels[i].rgb = rgb;
    }
}
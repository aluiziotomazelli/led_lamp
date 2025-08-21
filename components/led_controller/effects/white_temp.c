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
    // Predefined values represent different white color temperatures
    switch (temp_index) {
    case 0: // Very warm white (2200K-2700K equivalent)
        rgb = (rgb_t){255, 130, 30};
        break;
    case 1: // Warm white (3000K equivalent)
        rgb = (rgb_t){255, 140, 50};
        break;
    case 2: // Neutral white (3500K-4000K equivalent)
        rgb = (rgb_t){255, 197, 143};
        break;
    case 3: // Cool white (4500K-5000K equivalent)
        rgb = (rgb_t){255, 214, 170};
        break;
    case 4: // Very cool white (5500K-6000K equivalent)
        rgb = (rgb_t){255, 255, 255};
        break;
    case 5: // Ice cold white (6500K+ equivalent, bluish tint)
        rgb = (rgb_t){201, 226, 255};
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
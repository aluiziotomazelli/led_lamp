/**
 * @file white_temp.h
 * @brief White Temperature LED effect header with parameter definitions
 * 
 * @details Defines the white temperature effect parameters and function prototype
 *          for displaying different white color temperatures from warm to cool
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

#include "led_effects.h" // For effect_param_t, color_t

/**
 * @brief White temperature effect parameter configuration
 * 
 * @note Single parameter for selecting white color temperature from
 *       warm (0) to cool (5) with predefined RGB values
 */
static effect_param_t params_white_temp[] = {
    {.name = "Temperature",
     .type = PARAM_TYPE_VALUE,
     .value = 0,
     .min_value = 0,
     .max_value = 5,
     .step = 1,
     .is_wrap = false,
     .default_value = 0},
};

/**
 * @brief Runs the white temperature effect
 *
 * @param[in] params      Array of parameters for the effect
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds (unused in this effect)
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Applies a uniform white color temperature across all LEDs using
 *       predefined RGB values for different temperature levels
 * 
 * @warning Ensure num_params >= 1 to avoid parameter access violations
 * @note Time parameter is unused but maintained for interface consistency
 * @note Brightness control is handled by the LED controller, not this effect
 */
void run_white_temp(const effect_param_t *params, uint8_t num_params,
                    uint8_t brightness, uint64_t time_ms, color_t *pixels,
                    uint16_t num_pixels);
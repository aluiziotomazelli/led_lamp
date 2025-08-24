/**
 * @file christmas_tree.h
 * @brief Christmas Tree LED effect header with parameter definitions
 * 
 * @details Defines the Christmas tree effect parameters and function prototype
 *          for a festive holiday animation with background patterns and twinkling lights
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

#include "led_effects.h" // For effect_param_t, color_t

/**
 * @brief Christmas tree effect parameter configuration
 * 
 * @note Parameters for controlling twinkle speed and quantity of twinkling lights
 *       to create a festive holiday atmosphere
 */
static effect_param_t params_christmas_tree[] = {
    {.name = "Twinkle Speed",
     .type = PARAM_TYPE_SPEED,
     .value = 5,
     .min_value = 1,
     .max_value = 50,
     .step = 1,
     .is_wrap = false,
     .default_value = 5},
    {.name = "Twinkles",
     .type = PARAM_TYPE_VALUE,
     .value = 4,
     .min_value = 0,
     .max_value = 40, // Max 20 twinkles
     .step = 1,
     .is_wrap = false,
     .default_value = 4},
};

/**
 * @brief Runs the Christmas tree effect
 *
 * @param[in] params      Array of parameters for the effect
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Creates a festive Christmas tree effect with colored background segments,
 *       gentle global pulsation, and random twinkling white/gold lights
 * 
 * @warning Ensure num_params >= 2 to avoid parameter access violations
 */
void run_christmas_tree(const effect_param_t *params, uint8_t num_params,
                        uint8_t brightness, uint64_t time_ms, color_t *pixels,
                        uint16_t num_pixels);
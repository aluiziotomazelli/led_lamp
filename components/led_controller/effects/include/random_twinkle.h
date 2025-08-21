/**
 * @file random_twinkle.h
 * @brief Random Twinkle LED effect header with parameter definitions
 * 
 * @details Defines the random twinkle effect parameters and function prototype
 *          for a dynamic twinkling light animation with configurable probability
 *          and color palettes
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

#include "led_effects.h" // For effect_param_t, color_t

/**
 * @brief Random twinkle effect parameter configuration
 * 
 * @note Parameters for controlling twinkle probability, speed, maximum count,
 *       and color palette selection for versatile twinkling effects
 */
static effect_param_t params_random_twinkle[] = {
    {.name = "Probability",
     .type = PARAM_TYPE_VALUE,
     .value = 20,
     .min_value = 1,
     .max_value = 100,
     .step = 1,
     .is_wrap = false,
     .default_value = 20},
    {.name = "Speed",
     .type = PARAM_TYPE_SPEED,
     .value = 3,
     .min_value = 1,
     .max_value = 50,
     .step = 1,
     .is_wrap = false,
     .default_value = 3},
    {.name = "Max Twinkles",
     .type = PARAM_TYPE_VALUE,
     .value = 8,
     .min_value = 1,
     .max_value = 50,
     .step = 1,
     .is_wrap = false,
     .default_value = 10},
    {.name = "Palette",
     .type = PARAM_TYPE_VALUE,
     .value = 0,
     .min_value = 0,
     .max_value = 3,
     .step = 1,
     .is_wrap = false,
     .default_value = 0},
};

/**
 * @brief Runs the random twinkle effect
 *
 * @param[in] params      Array of parameters for the effect
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Creates a dynamic twinkling effect with independent LED activation,
 *       configurable probability, and multiple color palette options
 * 
 * @warning Ensure num_params >= 4 to avoid parameter access violations
 */
void run_random_twinkle(const effect_param_t *params, uint8_t num_params,
                        uint8_t brightness, uint64_t time_ms, color_t *pixels,
                        uint16_t num_pixels);
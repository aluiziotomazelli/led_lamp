/**
 * @file candle.h
 * @brief Candle flame LED effect header with parameter definitions
 * 
 * @details Defines the candle effect parameters and function prototype
 *          for a realistic flickering candle flame simulation
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

#include "led_effects.h" // For effect_param_t, color_t

/**
 * @brief Candle effect parameter configuration
 * 
 * @note Parameters for speed, hue, saturation, and segment control
 *       to create realistic candle flame flickering effects
 */
static effect_param_t params_candle[] = {
    {.name = "Speed",
     .type = PARAM_TYPE_SPEED,
     .value = 1,
     .min_value = 1,
     .max_value = 50,
     .step = 1,
     .is_wrap = false,
     .default_value = 1},
    {.name = "Hue",
     .type = PARAM_TYPE_HUE,
     .value = 35,
     .min_value = 5,
     .max_value = 80,
     .step = 1,
     .is_wrap = false,
     .default_value = 35},
    {.name = "Saturation",
     .type = PARAM_TYPE_SATURATION,
     .value = 255,
     .min_value = 0,
     .max_value = 255,
     .step = 1,
     .is_wrap = false,
     .default_value = 255},
    {.name = "Segments",
     .type = PARAM_TYPE_VALUE,
     .value = 4,
     .min_value = 1,
     .max_value = 10,
     .step = 1,
     .is_wrap = false,
     .default_value = 4},
};

/**
 * @brief Runs the candle effect
 *
 * @param[in] params      Array of parameters for the effect
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Simulates realistic candle flame flickering with random variations
 *       in brightness, hue, and saturation using precomputed noise tables
 * 
 * @warning Ensure num_params >= 4 to avoid parameter access violations
 */
void run_candle(const effect_param_t *params, uint8_t num_params,
                uint8_t brightness, uint64_t time_ms, color_t *pixels,
                uint16_t num_pixels);
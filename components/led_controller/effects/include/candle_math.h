/**
 * @file candle_math.h
 * @brief Mathematical candle flame LED effect header with parameter definitions
 * 
 * @details Defines the mathematical candle effect parameters and function prototype
 *          for a physically-based candle flame simulation using mathematical models
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

#include "led_effects.h" // For effect_param_t, color_t

/**
 * @brief Mathematical candle effect parameter configuration
 * 
 * @note Advanced parameters for speed, hue, saturation, segments, intensity,
 *       and dip probability for physically accurate candle flame simulation
 */
static effect_param_t params_candle_math[] = {
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
     .min_value = 0,
     .max_value = 359,
     .step = 1,
     .is_wrap = true,
     .default_value = 25},
    {.name = "Saturation",
     .type = PARAM_TYPE_SATURATION,
     .value = 240,
     .min_value = 0,
     .max_value = 255,
     .step = 1,
     .is_wrap = false,
     .default_value = 240},
    {.name = "Segments",
     .type = PARAM_TYPE_VALUE,
     .value = 4,
     .min_value = 1,
     .max_value = NUM_LEDS,
     .step = 1,
     .is_wrap = false,
     .default_value = 4},
    {.name = "Intensity",
     .type = PARAM_TYPE_VALUE,
     .value = 10,
     .min_value = 0,
     .max_value = 50,
     .step = 5,
     .is_wrap = false,
     .default_value = 30},
    {.name = "Dip Prob",
     .type = PARAM_TYPE_VALUE,
     .value = 3,
     .min_value = 0,
     .max_value = 100,
     .step = 1,
     .is_wrap = false,
     .default_value = 3},
};

/**
 * @brief Runs the mathematical candle effect
 *
 * @param[in] params      Array of parameters for the effect
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Uses mathematical models and state machines for physically accurate
 *       candle flame simulation with dynamic flickering and dip behaviors
 * 
 * @warning Ensure num_params >= 6 to avoid parameter access violations
 * @warning Requires candle_math_logic.h for the underlying implementation
 */
void run_candle_math(const effect_param_t *params, uint8_t num_params,
                     uint8_t brightness, uint64_t time_ms, color_t *pixels,
                     uint16_t num_pixels);
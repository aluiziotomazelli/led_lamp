/**
 * @file breathing.h
 * @brief Breathing LED effect header with parameter definitions
 * 
 * @details Defines the breathing effect parameters and function prototype
 *          for a smooth pulsating LED animation effect
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

#include "led_effects.h" // For effect_param_t, color_t

/**
 * @brief Breathing effect parameter configuration
 * 
 * @note Parameters for speed, hue, and saturation control
 *       the breathing animation characteristics
 */
static effect_param_t params_breathing[] = {
    {.name = "Speed",
     .type = PARAM_TYPE_SPEED,
     .value = 5,
     .min_value = 1,
     .max_value = 100,
     .step = 1,
     .is_wrap = false,
     .default_value = 5},
    {.name = "Hue",
     .type = PARAM_TYPE_HUE,
     .value = 250,
     .min_value = 0,
     .max_value = 359,
     .step = 1,
     .is_wrap = true,
     .default_value = 250},
    {.name = "Saturation",
     .type = PARAM_TYPE_SATURATION,
     .value = 255,
     .min_value = 0,
     .max_value = 255,
     .step = 5,
     .is_wrap = false,
     .default_value = 255},
};

/**
 * @brief Runs the breathing effect
 *
 * @param[in] params      Array of parameters for the effect
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Creates a smooth pulsating effect using sine wave modulation
 *       of brightness while maintaining constant hue and saturation
 */
void run_breathing(const effect_param_t *params, uint8_t num_params,
                   uint8_t brightness, uint64_t time_ms, color_t *pixels,
                   uint16_t num_pixels);
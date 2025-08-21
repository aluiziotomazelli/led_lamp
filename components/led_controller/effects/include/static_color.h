/**
 * @file static_color.h
 * @brief Static Color LED effect header with parameter definitions
 * 
 * @details Defines the static color effect parameters and function prototype
 *          for a solid, unchanging color display across all LEDs
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

#include "led_effects.h" // For effect_param_t, color_t

/**
 * @brief Static color effect parameter configuration
 * 
 * @note Parameters for controlling hue and saturation of the static color display
 *       with full wrap-around support for hue and clamped saturation
 */
static effect_param_t params_static_color[] = {
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
     .value = 230,
     .min_value = 0,
     .max_value = 255,
     .step = 5,
     .is_wrap = false,
     .default_value = 230},
};

/**
 * @brief Runs the static color effect
 *
 * @param[in] params      Array of parameters for the effect
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds (unused in this effect)
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Applies a uniform static color across all LEDs using HSV color model
 *       with master brightness control applied by the controller
 * 
 * @warning Ensure num_params >= 2 to avoid parameter access violations
 * @note Time parameter is unused but maintained for interface consistency
 */
void run_static_color(const effect_param_t *params, uint8_t num_params,
                      uint8_t brightness, uint64_t time_ms, color_t *pixels,
                      uint16_t num_pixels);
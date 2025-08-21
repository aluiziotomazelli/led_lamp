/**
 * @file led_effects.h
 * @brief LED Effects Engine definitions and data structures
 * 
 * @details This header provides the core data structures and type definitions
 *          for the LED effects system, including color representations,
 *          effect parameters, and effect function prototypes.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

// System includes
#include <stdint.h>
#include <stdbool.h>

// Project specific headers
#include "project_config.h"

// Forward declaration
struct effect_t;

//------------------------------------------------------------------------------
// COLOR DATA STRUCTURES
//------------------------------------------------------------------------------

/**
 * @brief RGB color structure
 * 
 * @details Represents a color in the RGB color space with 8-bit precision
 *          per channel (24-bit color depth).
 */
typedef struct {
    uint8_t r;  ///< Red component (0-255)
    uint8_t g;  ///< Green component (0-255)
    uint8_t b;  ///< Blue component (0-255)
} rgb_t;

/**
 * @brief HSV color structure
 * 
 * @details Represents a color in the HSV color space with hue in degrees
 *          and saturation/value as 8-bit values.
 */
typedef struct {
    uint16_t h;  ///< Hue: 0-359 degrees
    uint8_t s;   ///< Saturation: 0-255
    uint8_t v;   ///< Value (brightness): 0-255
} hsv_t;

/**
 * @brief Union to hold either RGB or HSV color
 * 
 * @details Allows storing color data in either RGB or HSV format
 *          while using the same memory space.
 */
typedef union {
    rgb_t rgb;  ///< RGB color representation
    hsv_t hsv;  ///< HSV color representation
} color_t;

//------------------------------------------------------------------------------
// ENUMERATIONS
//------------------------------------------------------------------------------

/**
 * @brief Enum to indicate the color mode used by an effect
 * 
 * @details Specifies whether an effect outputs colors in RGB or HSV format.
 */
typedef enum {
    COLOR_MODE_RGB,  ///< Effect outputs RGB colors
    COLOR_MODE_HSV,  ///< Effect outputs HSV colors
} color_mode_t;

/**
 * @brief Enum for parameter types
 * 
 * @details Categorizes effect parameters for proper UI handling and validation.
 */
typedef enum {
    PARAM_TYPE_VALUE,       ///< Generic numeric value
    PARAM_TYPE_HUE,         ///< Hue parameter (0-359 degrees)
    PARAM_TYPE_SATURATION,  ///< Saturation parameter (0-255)
    PARAM_TYPE_BRIGHTNESS,  ///< Brightness parameter (0-255)
    PARAM_TYPE_SPEED,       ///< Animation speed parameter
    PARAM_TYPE_BOOLEAN,     ///< Boolean (on/off) parameter
} param_type_t;

//------------------------------------------------------------------------------
// EFFECT PARAMETER STRUCTURE
//------------------------------------------------------------------------------

/**
 * @brief Effect parameter structure
 * 
 * @details Defines a configurable parameter for an LED effect, including
 *          validation ranges, step size, and default values.
 */
typedef struct {
    const char *name;          ///< Human-readable parameter name
    param_type_t type;         ///< Parameter type for UI handling
    int16_t value;             ///< Current parameter value
    int16_t min_value;         ///< Minimum allowed value
    int16_t max_value;         ///< Maximum allowed value
    int16_t step;              ///< Step size for increment/decrement
    bool is_wrap;              ///< Whether value wraps around at limits
    int16_t default_value;     ///< Factory default value
} effect_param_t;

//------------------------------------------------------------------------------
// EFFECT FUNCTION PROTOTYPES
//------------------------------------------------------------------------------

/**
 * @brief Function pointer type for running an effect
 * 
 * @details This function type defines the signature for all effect
 *          implementation functions.
 *
 * @param[in] params Array of parameters for the effect
 * @param[in] num_params Number of parameters in the array
 * @param[in] brightness Master brightness value (0-255)
 * @param[in] time_ms Current time in milliseconds for animation timing
 * @param[out] pixels Output pixel buffer (array of color_t)
 * @param[in] num_pixels Number of pixels in the buffer
 */
typedef void (*effect_run_t)(const effect_param_t *params, uint8_t num_params, 
                            uint8_t brightness, uint64_t time_ms, 
                            color_t *pixels, uint16_t num_pixels);

//------------------------------------------------------------------------------
// EFFECT DEFINITION STRUCTURE
//------------------------------------------------------------------------------

/**
 * @brief Effect definition structure
 * 
 * @details Complete definition of an LED effect, including its execution
 *          function, parameters, and metadata.
 */
typedef struct effect_t {
    const char *name;          ///< Human-readable effect name
    effect_run_t run;          ///< Pointer to the effect execution function
    color_mode_t color_mode;   ///< Color mode this effect outputs (RGB/HSV)
    effect_param_t *params;    ///< Array of configurable parameters
    uint8_t num_params;        ///< Number of parameters in the array
    bool is_dynamic;           ///< Whether the effect has animation/movement
} effect_t;
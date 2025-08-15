#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "project_config.h"


// Forward declaration
struct effect_t;

/**
 * @brief RGB color structure
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

/**
 * @brief HSV color structure
 */
typedef struct {
    uint16_t h; // Hue: 0-359
    uint8_t s;  // Saturation: 0-255
    uint8_t v;  // Value: 0-255
} hsv_t;

/**
 * @brief Union to hold either RGB or HSV color
 */
typedef union {
    rgb_t rgb;
    hsv_t hsv;
} color_t;

/**
 * @brief Enum to indicate the color mode used by an effect
 */
typedef enum {
    COLOR_MODE_RGB,
    COLOR_MODE_HSV,
} color_mode_t;

/**
 * @brief Enum for parameter types
 */
typedef enum {
    PARAM_TYPE_VALUE,
    PARAM_TYPE_HUE,
    PARAM_TYPE_SATURATION,
    PARAM_TYPE_BRIGHTNESS,
    PARAM_TYPE_SPEED,
    PARAM_TYPE_BOOLEAN,
} param_type_t;

/**
 * @brief Effect parameter structure
 */
typedef struct {
    const char *name;
    param_type_t type;
    int16_t value;
    int16_t min_value;
    int16_t max_value;
    int16_t step;
} effect_param_t;

/**
 * @brief Function pointer for running an effect
 *
 * @param params Array of parameters for the effect
 * @param num_params Number of parameters
 * @param brightness Master brightness (0-255)
 * @param time_ms Current time in milliseconds
 * @param pixels Output pixel buffer (array of color_t)
 * @param num_pixels Number of pixels in the buffer
 */
typedef void (*effect_run_t)(const effect_param_t *params, uint8_t num_params, uint8_t brightness, uint64_t time_ms, color_t *pixels, uint16_t num_pixels);

/**
 * @brief Effect definition structure
 */
typedef struct effect_t {
    const char *name;
    effect_run_t run;
    color_mode_t color_mode; // The color mode this effect outputs
    effect_param_t *params;
    uint8_t num_params;
    bool is_dynamic;
} effect_t;

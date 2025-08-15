#pragma once

#include "project_config.h"
#include <stdbool.h>
#include <stdint.h>

#define CANDLE_TABLE_SIZE                                                      \
	10486 // Tamanho da tabela de brilho para o efeito vela

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
	uint16_t h; // 0-360
	uint8_t s;	// 0-100
	uint8_t v;	// 0-100
} hsv_t;

typedef union {
	rgb_t rgb;
	hsv_t hsv;
} color_t;

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
 * @param pixels Output pixel buffer
 * @param num_pixels Number of pixels in the buffer
 */
typedef void (*effect_run_t)(const effect_param_t *params, uint8_t num_params,
							 uint8_t brightness, uint64_t time_ms,
							 color_t *pixels, uint16_t num_pixels);

typedef enum { PIXEL_FORMAT_RGB, PIXEL_FORMAT_HSV } pixel_format_t;

/**
 * @brief Effect definition structure
 */
typedef struct effect_t {
	const char *name;
	effect_run_t run;
	effect_param_t *params;
	uint8_t num_params;
	pixel_format_t pixel_format;
	bool is_static;
} effect_t;

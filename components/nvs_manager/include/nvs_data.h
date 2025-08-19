/**
 * @file nvs_data.h
 * @brief NVS data structures and definitions for LED configuration storage
 * 
 * @details This header defines the data structures used for storing both volatile
 *          and static configuration data in non-volatile storage (NVS).
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

// System includes
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Define the dimensions of the effect parameters array
 * 
 * @note These values are derived from `led_effects.c`
 *       NUM_EFFECTS corresponds to `effects_count`
 *       MAX_PARAMS_PER_EFFECT is the highest number of parameters for any effect
 */
#define NVS_NUM_EFFECTS 6                    ///< Maximum number of supported effects
#define NVS_MAX_PARAMS_PER_EFFECT 8          ///< Maximum parameters per effect (actual max is 6)

/**
 * @brief Structure for data that changes frequently
 * 
 * @details This data is saved on a timer or specific volatile events
 */
typedef struct {
    bool is_on;                             ///< Power state (On/Off)
    uint8_t master_brightness;              ///< Master brightness (0-255)
    uint8_t effect_index;                   ///< Index of the current effect
} volatile_data_t;

/**
 * @brief Structure for data that changes infrequently
 * 
 * @details This data is saved immediately upon modification through configuration menu
 */
typedef struct {
    // System-wide settings
    uint8_t min_brightness;                 ///< Minimum brightness level for effects
    uint16_t led_offset_begin;              ///< Number of LEDs to skip at the start
    uint16_t led_offset_end;                ///< Number of LEDs to skip at the end

    // Per-effect settings
    int16_t effect_params[NVS_NUM_EFFECTS][NVS_MAX_PARAMS_PER_EFFECT]; ///< Storage for each parameter's value
} static_data_t;
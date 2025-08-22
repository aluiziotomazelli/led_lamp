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
    uint8_t correction_r;                   ///< Red channel color correction
    uint8_t correction_g;                   ///< Green channel color correction
    uint8_t correction_b;                   ///< Blue channel color correction

    // Per-effect settings
    int16_t effect_params[NVS_NUM_EFFECTS][NVS_MAX_PARAMS_PER_EFFECT]; ///< Storage for each parameter's value
} static_data_t;

/**
 * @brief Structure for OTA configuration data
 *
 * @details This data is saved when the user enters OTA mode
 */
typedef struct {
    bool ota_mode_enabled;                  ///< Flag to indicate if OTA mode should be entered on boot
    char wifi_ssid[33];                     ///< Wi-Fi SSID (max 32 chars + null)
    char wifi_password[65];                 ///< Wi-Fi Password (max 64 chars + null)
} ota_data_t;
/**
 * @file candle_math.h
 * @brief Candle Effect Mathematical Model and Configuration
 * 
 * @details This header provides the interface for a realistic candle flame simulation
 *          effect using mathematical models for flickering, color variation, and
 *          zone-based brightness control. Supports configurable parameters for
 *          fine-tuning the candle appearance.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

// System includes
#include <stdint.h>

// Project specific headers
#include "led_effects.h" // For color_t

/**
 * @brief Candle effect configuration structure
 * 
 * @details Contains all configurable parameters for tuning the candle flame behavior,
 *          including flickering characteristics, brightness ranges, and color variations.
 */
typedef struct {
    // Zone configuration
    uint16_t num_zones;           ///< Number of independent flame zones
    uint16_t leds_per_zone;       ///< Number of LEDs per zone
    
    // Flicker behavior parameters
    float flicker_speed;          ///< Speed of flicker oscillations (recommended: 0.05f)
    float dip_probability;        ///< Probability of brightness dips (recommended: 0.01f)
    float recovery_rate;          ///< Rate of brightness recovery after dips (recommended: 0.05f)
    
    // Brightness range parameters
    float min_brightness;         ///< Minimum brightness level (recommended: 20.0f)
    float max_brightness;         ///< Maximum brightness level (recommended: 100.0f)
    float base_brightness;        ///< Base brightness level (recommended: 70.0f)
    float flicker_intensity;      ///< Intensity of flicker variation (recommended: 0.2f)

    // Color configuration parameters
    uint16_t base_hue;            ///< Base hue value for candle color
    uint16_t min_hue;             ///< Minimum hue variation
    uint16_t max_hue;             ///< Maximum hue variation
    uint16_t base_sat;            ///< Base saturation value
    uint16_t min_sat;             ///< Minimum saturation variation
    uint16_t max_sat;             ///< Maximum saturation variation
} candle_config_t;

/**
 * @brief Candle effect instance structure
 * 
 * @details Contains the runtime state of a candle effect instance, including
 *          configuration, zone brightness values, and global brightness state.
 */
typedef struct {
    candle_config_t config;       ///< Effect configuration parameters
    float* zone_brightness;       ///< Dynamic brightness values for each zone
    float global_brightness;      ///< Global brightness level for overall effect
} candle_effect_t;


//------------------------------------------------------------------------------
// CANDLE EFFECT MANAGEMENT FUNCTIONS
//------------------------------------------------------------------------------

/**
 * @brief Initialize a new candle effect instance
 * 
 * @details Allocates and initializes a candle effect with the given configuration.
 *          Creates brightness buffers for each zone and sets initial values.
 *
 * @param[in] config Pointer to candle configuration structure
 * @return candle_effect_t* Pointer to initialized effect instance
 * 
 * @note Returns NULL on memory allocation failure
 * @warning Caller is responsible for deinitialization with candle_effect_deinit()
 */
candle_effect_t* candle_effect_init(const candle_config_t* config);

/**
 * @brief Update the candle effect simulation
 * 
 * @details Advances the candle simulation by the specified time delta, updating
 *          zone brightness values, applying flicker effects, and generating
 *          pixel colors for the LED strip.
 *
 * @param[in] effect Pointer to candle effect instance
 * @param[in] delta_time Time elapsed since last update (in seconds)
 * @param[out] pixels Output pixel buffer for rendered colors
 * @param[in] num_pixels Number of pixels in the output buffer
 * 
 * @note The pixel buffer must have sufficient capacity for num_pixels
 */
void candle_effect_update(candle_effect_t* effect, float delta_time, color_t *pixels, uint16_t num_pixels);

/**
 * @brief Deinitialize and cleanup candle effect instance
 * 
 * @details Releases all allocated memory associated with the candle effect,
 *          including zone brightness buffers.
 *
 * @param[in] effect Pointer to candle effect instance to cleanup
 * 
 * @note Safe to call with NULL pointer (no operation)
 */
void candle_effect_deinit(candle_effect_t* effect);
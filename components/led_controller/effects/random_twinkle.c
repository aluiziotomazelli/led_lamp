/**
 * @file random_twinkle.c
 * @brief Random Twinkle LED effect implementation
 * 
 * @details Implements a dynamic random twinkling effect with probability-based
 *          activation, configurable color palettes, and independent LED state
 *          management for organic lighting patterns
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// Project specific headers
#include "led_effects.h" // For color_t, effect_param_t, etc.

// Standard library includes
#include <stdint.h>
#include <stdlib.h> // For calloc, free, rand
#include <stdbool.h>
#include <math.h> // For abs

/**
 * @brief Selects a random color from the specified palette
 * 
 * @param[in] palette Palette index (0-3) determining color options
 * @param[out] c Pointer to color structure to be filled
 * 
 * @note Palette options:
 *       0: Gold only
 *       1: Gold + White  
 *       2: Gold + White + Red
 *       3: Gold + White + Red + Green (full Christmas palette)
 */
static void pick_twinkle_color(uint8_t palette, color_t *c) {
    // HSV definitions for each color
    // Hue in degrees (0..360), saturation/value 0..255
    switch (palette) {
        case 0: { // Gold only
            c->hsv.h = 40;  // Gold
            c->hsv.s = 240;
            c->hsv.v = 255;
            break;
        }
        case 1: { // Gold + White
            int r = rand() % 2;
            if (r == 0) { // Gold
                c->hsv.h = 40; c->hsv.s = 240; c->hsv.v = 255;
            } else {      // White
                c->hsv.h = 0;  c->hsv.s = 0;   c->hsv.v = 255;
            }
            break;
        }
        case 2: { // Gold + White + Red
            int r = rand() % 3;
            if (r == 0) { // Gold
                c->hsv.h = 40; c->hsv.s = 240; c->hsv.v = 255;
            } else if (r == 1) { // White
                c->hsv.h = 0;  c->hsv.s = 0;   c->hsv.v = 255;
            } else { // Red
                c->hsv.h = 0;  c->hsv.s = 255; c->hsv.v = 255;
            }
            break;
        }
        default: { // Gold + White + Red + Green (full palette)
            int r = rand() % 4;
            if (r == 0) { // Gold
                c->hsv.h = 40; c->hsv.s = 240; c->hsv.v = 255;
            } else if (r == 1) { // White
                c->hsv.h = 0;  c->hsv.s = 0;   c->hsv.v = 255;
            } else if (r == 2) { // Red
                c->hsv.h = 0;  c->hsv.s = 255; c->hsv.v = 255;
            } else { // Green
                c->hsv.h = 120; c->hsv.s = 255; c->hsv.v = 255;
            }
            break;
        }
    }
}

/**
 * @brief Runs the random twinkle effect algorithm
 * 
 * @param[in] params      Array of effect parameters (probability, speed, max_twinkles, palette)
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Uses probability-based activation with cooldown periods and triangular
 *       wave brightness modulation for organic twinkling patterns
 * 
 * @warning Maintains persistent state between calls - reinitializes on LED count changes
 */
void run_random_twinkle(const effect_param_t *params, uint8_t num_params,
                        uint8_t brightness, uint64_t time_ms,
                        color_t *pixels, uint16_t num_pixels) {
    /**
     * @brief Random twinkle LED state structure
     * 
     * @details Tracks individual LED animation state including phase,
     *          activation status, cooldown timer, and persistent color
     */
    typedef struct {
        int phase;          ///< Animation phase (-255 to 255) for triangular wave
        bool active;        ///< Whether the LED is currently twinkling
        uint8_t cooldown;   ///< Cooldown frames before reactivation
        color_t color;      ///< Persistent color for this twinkle
    } random_twinkle_t;

    // Static effect state variables
    static random_twinkle_t *twinkle_state = NULL;  ///< Array of LED states
    static uint16_t twinkle_num_leds2 = 0;          ///< Current number of LEDs in state array

    // Extract effect parameters
    uint8_t probability = params[0].value; // 1..100 (% chance per frame)
    uint8_t speed       = params[1].value; // 1..50 (animation speed)
    uint8_t max_active  = params[2].value; // 1..20 (maximum simultaneous twinkles)

    // (Re)initialize state if LED count changed
    if (twinkle_state == NULL || twinkle_num_leds2 != num_pixels) {
        if (twinkle_state) free(twinkle_state);
        
        // Allocate and initialize state array
        twinkle_state = calloc(num_pixels, sizeof(random_twinkle_t));
        twinkle_num_leds2 = num_pixels;
        
        // Initialize all LEDs to inactive state
        for (uint16_t i = 0; i < num_pixels; i++) {
            twinkle_state[i].phase = -255;
            twinkle_state[i].active = false;
            twinkle_state[i].cooldown = 0;
        }
    }

    // 1) Clear the frame (black background)
    for (uint16_t i = 0; i < num_pixels; i++) {
        pixels[i].hsv.h = 0;
        pixels[i].hsv.s = 0;
        pixels[i].hsv.v = 0;
    }

    // 2) Update active twinkles and count how many remain active
    uint16_t active_count = 0;
    for (uint16_t i = 0; i < num_pixels; i++) {
        random_twinkle_t *s = &twinkle_state[i];

        if (s->active) {
            // Calculate brightness using triangular wave (0-255)
            int b = 255 - abs(s->phase);
            
            // Apply twinkle color with brightness modulation
            pixels[i].hsv.h = s->color.hsv.h;
            pixels[i].hsv.s = s->color.hsv.s;
            pixels[i].hsv.v = (b * s->color.hsv.v) / 255;

            // Advance animation phase
            s->phase += speed;
            
            // Check if animation cycle completed
            if (s->phase > 255) {
                // Deactivate and set cooldown period
                s->active = false;
                s->phase = -255;
                s->cooldown = 2 + (rand() % 4); // 2..5 frame cooldown
            } else {
                active_count++;
            }
        } else {
            // Decrement cooldown timer
            if (s->cooldown > 0) s->cooldown--;
        }
    }

    // 3) Spawn new twinkles using global random selection (no index bias)
    if (active_count < max_active && probability > 0) {
        uint16_t capacity = max_active - active_count;

        // Estimate how many new twinkles to spawn this frame
        uint16_t inactive = num_pixels - active_count;
        uint16_t target_new = (inactive * probability + 99) / 100; // Round up
        if (target_new > capacity) target_new = capacity;
        
        // Ensure at least one twinkle can spawn with low probabilities
        if (target_new == 0 && capacity > 0 && (rand() % 100) < probability) {
            target_new = 1;
        }

        // Activate target_new LEDs by selecting random indices, respecting cooldown
        uint16_t spawned = 0;
        uint32_t max_tries = target_new * 8 + 16; // Limit attempts to avoid long loops
        while (spawned < target_new && max_tries--) {
            uint16_t idx = (num_pixels > 0) ? (rand() % num_pixels) : 0;
            random_twinkle_t *s = &twinkle_state[idx];
            
            // Activate if eligible (inactive and cooldown expired)
            if (!s->active && s->cooldown == 0) {
                s->active = true;
                s->phase = -255;
                pick_twinkle_color(params[3].value, &s->color); // Use palette parameter
                spawned++;
            }
        }
    }
}
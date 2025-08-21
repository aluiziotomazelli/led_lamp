/**
 * @file christmas_twinkle.c
 * @brief Christmas Twinkle LED effect implementation
 * 
 * @details Implements a classic Christmas light twinkling effect with
 *          independent LED brightness modulation and random color selection
 *          for a traditional holiday lighting appearance
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
#include <math.h>   // For abs

/* --- Effect: Christmas Twinkle --- */

/**
 * @brief Christmas twinkle LED state structure
 * 
 * @details Tracks individual LED state including animation speed,
 *          brightness phase, and base color for twinkling effect
 */
typedef struct {
    uint8_t inc;     ///< Animation speed increment
    int dim;         ///< Brightness phase (-255 to 255)
    color_t base;    ///< Base color in RGB format
} christmas_twinkle_t;

// Static effect state variables
static christmas_twinkle_t *twinkle_states = NULL; ///< Array of LED states
static uint16_t twinkle_num_leds = 0;              ///< Current number of LEDs in state array

/**
 * @brief Chooses a new random Christmas color for an LED
 * 
 * @param[in,out] s Pointer to twinkle state to update
 * 
 * @note Selects from traditional Christmas colors: red, green, or white
 *       with specific RGB values for authentic appearance
 */
static void choose_new_color(christmas_twinkle_t *s) {
    int choice = rand() % 3;
    switch (choice) {
    case 0: // Classic Christmas red
        s->base.rgb.r = 255;
        s->base.rgb.g = 0;
        s->base.rgb.b = 18;
        break;
    case 1: // Classic Christmas green
        s->base.rgb.r = 0;
        s->base.rgb.g = 179;
        s->base.rgb.b = 44;
        break;
    default: // Bright white
        s->base.rgb.r = 255;
        s->base.rgb.g = 255;
        s->base.rgb.b = 255;
        break;
    }
}

/**
 * @brief Runs the Christmas twinkle effect algorithm
 * 
 * @param[in] params      Array of effect parameters (speed, density)
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Uses independent state tracking for each LED with triangular wave
 *       brightness modulation and random color selection for authentic effect
 * 
 * @warning Maintains persistent state between calls - reinitializes on LED count changes
 */
void run_christmas_twinkle(const effect_param_t *params,
                           uint8_t num_params, uint8_t brightness,
                           uint64_t time_ms, color_t *pixels,
                           uint16_t num_pixels) {
    // Extract effect parameters
    uint8_t speed = params[0].value;
    uint8_t density = params[1].value; // Controls how many LEDs twinkle simultaneously

    // State initialization (if LED count changed)
    if (twinkle_states == NULL || twinkle_num_leds != num_pixels) {
        if (twinkle_states)
            free(twinkle_states);
        
        // Allocate state array for all LEDs
        twinkle_states = calloc(num_pixels, sizeof(christmas_twinkle_t));
        twinkle_num_leds = num_pixels;

        // Initialize each LED with random properties
        for (uint16_t i = 0; i < num_pixels; i++) {
            twinkle_states[i].inc = (rand() % 8) + 1;        // Random speed (1-8)
            twinkle_states[i].dim = (rand() % 511) - 255;    // Random phase (-255 to 255)
            choose_new_color(&twinkle_states[i]);            // Random Christmas color
        }
    }

    // Process each pixel to generate twinkling effect
    for (uint16_t i = 0; i < num_pixels; i++) {
        christmas_twinkle_t *s = &twinkle_states[i];

        // Calculate local brightness using triangular wave (0-255)
        // abs(dim) creates the V-shaped brightness curve
        int brightness_local = 255 - abs(s->dim);

        // Apply density control: only fully activate LEDs below density threshold
        // This simulates fewer lights twinkling simultaneously
        if ((i % (20 / density + 1)) != 0) {
            brightness_local = brightness_local / 4; // Dim inactive LEDs
        }

        // Apply brightness scaling to base color
        pixels[i].rgb.r = (s->base.rgb.r * brightness_local) / 255;
        pixels[i].rgb.g = (s->base.rgb.g * brightness_local) / 255;
        pixels[i].rgb.b = (s->base.rgb.b * brightness_local) / 255;

        // Advance animation state
        s->dim += (s->inc * speed) / 10;
        
        // Reset phase and choose new color when cycle completes
        if (s->dim > 255) {
            s->dim = -255;
            choose_new_color(s);
        }
    }
}
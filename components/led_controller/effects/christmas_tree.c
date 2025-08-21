/**
 * @file christmas_tree.c
 * @brief Christmas Tree LED effect implementation
 * 
 * @details Implements a festive Christmas tree animation with colored background
 *          segments, gentle global pulsation, and random twinkling lights
 *          for a holiday-themed lighting effect
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// Project specific headers
#include "led_effects.h" // For color_t, effect_param_t, etc.

// Standard library includes
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> // For rand

// --- Static state for the effect ---

/**
 * @brief Maximum number of simultaneous twinkling lights
 */
#define MAX_TWINKLES 20

/**
 * @brief Twinkle light state structure
 * 
 * @details Tracks individual twinkling light properties including
 *          position, color, timing, and activation state
 */
typedef struct {
    bool is_active;         ///< Whether this twinkle is currently active
    int16_t led_index;      ///< LED index where the twinkle is displayed
    hsv_t color;            ///< Color of the twinkle light
    uint64_t start_time;    ///< Start time of the twinkle animation
    uint16_t duration_ms;   ///< Total duration of the twinkle effect
} twinkle_t;

// Static effect state variables
static hsv_t background_pattern[NUM_LEDS];  ///< Pre-generated background color pattern
static bool background_initialized = false; ///< Background pattern initialization flag
static twinkle_t twinkles[MAX_TWINKLES];    ///< Array of twinkle light states
static bool twinkles_initialized = false;   ///< Twinkle array initialization flag

/**
 * @brief Runs the Christmas tree effect algorithm
 * 
 * @param[in] params      Array of effect parameters (twinkle speed, count)
 * @param[in] num_params  Number of parameters in the array
 * @param[in] brightness  Master brightness level (0-255)
 * @param[in] time_ms     Current time in milliseconds for animation timing
 * @param[out] pixels     Output pixel buffer to be filled with colors
 * @param[in] num_pixels  Number of pixels in the output buffer
 * 
 * @note Creates a three-layer effect: background pattern, global pulsation,
 *       and overlay twinkling lights for a complete Christmas tree appearance
 * 
 * @warning Uses static state that persists between calls for consistent animation
 */
void run_christmas_tree(const effect_param_t *params, uint8_t num_params,
                        uint8_t brightness, uint64_t time_ms, color_t *pixels,
                        uint16_t num_pixels) {
    // Get parameters from the UI
    uint8_t twinkle_speed = params[0].value;
    uint8_t num_twinkles = params[1].value;

    // --- 1. Generate and Draw Background ---
    // On first run, generate a fixed random pattern of colored segments
    if (!background_initialized) {
        // Base Christmas colors: green, red, gold
        hsv_t base_colors[] = {
            {.h = 120, .s = 255, .v = 150}, // Green
            {.h = 0, .s = 255, .v = 150},   // Red
            {.h = 40, .s = 220, .v = 150}   // Gold
        };
        int color_count = sizeof(base_colors) / sizeof(hsv_t);
        int color_index = rand() % color_count; // Start with a random color

        uint16_t i = 0;
        while (i < num_pixels) {
            // Segments are 3 to 5 LEDs long for natural appearance
            uint8_t segment_length = 3 + (rand() % 3);

            // Get base color and apply slight random variations for natural look
            hsv_t segment_color = base_colors[color_index % color_count];
            segment_color.h = (segment_color.h + (rand() % 10) - 5) % 360;
            segment_color.v = segment_color.v + (rand() % 20) - 10;

            // Fill the segment with the generated color
            for (uint8_t j = 0; j < segment_length && i < num_pixels;
                 j++, i++) {
                if (i < NUM_LEDS) { // Safety check against buffer size
                    background_pattern[i] = segment_color;
                }
            }
            color_index++;
        }
        background_initialized = true;
    }

    // Copy the pre-generated background pattern to the active pixel buffer
    for (uint16_t i = 0; i < num_pixels; i++) {
        if (i < NUM_LEDS) { // Safety check
            pixels[i].hsv = background_pattern[i];
        }
    }

    // --- 2. Add Global Brightness Pulsation ---
    // A very slow and subtle sine wave to make the whole strip gently
    // "breathe" like a real Christmas tree
    float pulse_wave = (sinf(time_ms / 4000.0f) * 0.15f) +
                       0.85f; // Varies between 85% and 100%

    for (uint16_t i = 0; i < num_pixels; i++) {
        uint8_t base_v = pixels[i].hsv.v;
        pixels[i].hsv.v = (uint8_t)(base_v * pulse_wave);
    }

    // --- 3. Twinkling Overlay ---
    // Initialize the twinkle state array on the first run
    if (!twinkles_initialized) {
        for (int i = 0; i < MAX_TWINKLES; i++) {
            twinkles[i].is_active = false;
        }
        twinkles_initialized = true;
    }

    // Animate and draw existing twinkles
    for (int i = 0; i < MAX_TWINKLES; i++) {
        if (twinkles[i].is_active) {
            uint64_t elapsed = time_ms - twinkles[i].start_time;

            if (elapsed >= twinkles[i].duration_ms) {
                twinkles[i].is_active = false; // Deactivate if its life is over
                continue;
            }

            // Use a triangular wave for a smooth fade-in and fade-out
            // brightness curve (peak at 50% duration)
            float progress = (float)elapsed / (float)twinkles[i].duration_ms;
            float brightness_multiplier = (progress < 0.5f)
                                              ? (progress * 2.0f)
                                              : ((1.0f - progress) * 2.0f);

            hsv_t twinkle_color = twinkles[i].color;
            twinkle_color.v = (uint8_t)(255 * brightness_multiplier);

            // Overlay the twinkle on the background, but only if it's brighter
            // to avoid darkening existing lights
            if (twinkles[i].led_index < num_pixels && twinkle_color.v > pixels[twinkles[i].led_index].hsv.v) {
                pixels[twinkles[i].led_index].hsv = twinkle_color;
            }
        }
    }

    // Count active twinkles to see if we need to spawn new ones
    int active_count = 0;
    for (int i = 0; i < MAX_TWINKLES; i++) {
        if (twinkles[i].is_active)
            active_count++;
    }

    // If we have fewer active twinkles than the user requested, spawn a new one
    if (active_count < num_twinkles) {
        for (int i = 0; i < MAX_TWINKLES; i++) {
            if (!twinkles[i].is_active) {
                // Initialize new twinkle properties
                twinkles[i].is_active = true;
                twinkles[i].led_index = rand() % num_pixels;
                twinkles[i].start_time = time_ms;
                
                // Duration is inversely related to the "speed" parameter
                // Faster speed = shorter duration for more rapid twinkling
                twinkles[i].duration_ms =
                    (51 - twinkle_speed) * 40 + (rand() % 500);

                // Set twinkle color (mostly white or gold for a classic look)
                if ((rand() % 10) < 6) { // 60% chance of White/Silver
                    twinkles[i].color = (hsv_t){.h = 0, .s = 0, .v = 255};
                } else { // 40% chance of Gold
                    twinkles[i].color = (hsv_t){.h = 40, .s = 180, .v = 255};
                }
                break; // Spawn only one new twinkle per frame to avoid clumping
            }
        }
    }
}
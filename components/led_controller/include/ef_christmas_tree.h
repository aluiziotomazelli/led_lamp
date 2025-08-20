#pragma once

// System includes
#include <math.h>
#include <stdint.h>

// Project specific headers
#include "led_effects.h" // For color_t

/* --- Effect: Christmas --- */

static effect_param_t params_christmas_tree[] = {
	{.name = "Twinkle Speed",
	 .type = PARAM_TYPE_SPEED,
	 .value = 5,
	 .min_value = 1,
	 .max_value = 50,
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 5},
	{.name = "Twinkles",
	 .type = PARAM_TYPE_VALUE,
	 .value = 4,
	 .min_value = 0,
	 .max_value = 20, // Max 20 twinkles
	 .step = 1,
	 .is_wrap = false,
	 .default_value = 4},
};

static void run_christmas_tree(const effect_param_t *params, uint8_t num_params,
						  uint8_t brightness, uint64_t time_ms, color_t *pixels,
						  uint16_t num_pixels) {
	// Get parameters from the UI
	uint8_t twinkle_speed = params[0].value;
	uint8_t num_twinkles = params[1].value;

	// --- 1. Generate and Draw Background ---
	static hsv_t background_pattern[NUM_LEDS];
	static bool background_initialized = false;

	// On first run, generate a fixed random pattern of colored segments
	if (!background_initialized) {
		hsv_t base_colors[] = {
			{.h = 120, .s = 255, .v = 150}, // Green
			{.h = 0, .s = 255, .v = 150},	// Red
			{.h = 40, .s = 220, .v = 150}	// Gold
		};
		int color_count = sizeof(base_colors) / sizeof(hsv_t);
		int color_index = rand() % color_count; // Start with a random color

		uint16_t i = 0;
		while (i < num_pixels) {
			// Segments are 3 to 5 LEDs long
			uint8_t segment_length = 3 + (rand() % 3);

			// Get base color and apply slight random variations
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
	// "breathe".
	float pulse_wave = (sinf(time_ms / 4000.0f) * 0.15f) +
					   0.85f; // Varies between 85% and 100%

	for (uint16_t i = 0; i < num_pixels; i++) {
		uint8_t base_v = pixels[i].hsv.v;
		pixels[i].hsv.v = (uint8_t)(base_v * pulse_wave);
	}

// --- 3. Twinkling Overlay ---
#define MAX_TWINKLES 20
	typedef struct {
		bool is_active;
		int16_t led_index;
		hsv_t color;
		uint64_t start_time;
		uint16_t duration_ms;
	} twinkle_t;

	static twinkle_t twinkles[MAX_TWINKLES];
	static bool initialized = false;

	// Initialize the twinkle state array on the first run.
	if (!initialized) {
		for (int i = 0; i < MAX_TWINKLES; i++) {
			twinkles[i].is_active = false;
		}
		initialized = true;
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
			// brightness curve.
			float progress = (float)elapsed / (float)twinkles[i].duration_ms;
			float brightness_multiplier = (progress < 0.5f)
											  ? (progress * 2.0f)
											  : ((1.0f - progress) * 2.0f);

			hsv_t twinkle_color = twinkles[i].color;
			twinkle_color.v = (uint8_t)(255 * brightness_multiplier);

			// Overlay the twinkle on the background, but only if it's brighter.
			if (twinkle_color.v > pixels[twinkles[i].led_index].hsv.v) {
				pixels[twinkles[i].led_index].hsv = twinkle_color;
			}
		}
	}

	// Count active twinkles to see if we need to spawn new ones.
	int active_count = 0;
	for (int i = 0; i < MAX_TWINKLES; i++) {
		if (twinkles[i].is_active)
			active_count++;
	}

	// If we have fewer active twinkles than the user requested, spawn a new
	// one.
	if (active_count < num_twinkles) {
		for (int i = 0; i < MAX_TWINKLES; i++) {
			if (!twinkles[i].is_active) {
				twinkles[i].is_active = true;
				twinkles[i].led_index = rand() % num_pixels;
				twinkles[i].start_time = time_ms;
				// Duration is inversely related to the "speed" parameter.
				// Faster speed = shorter duration.
				twinkles[i].duration_ms =
					(51 - twinkle_speed) * 40 + (rand() % 500);

				// Set twinkle color (mostly white or gold for a classic look)
				if ((rand() % 10) < 6) { // 60% chance of White/Silver
					twinkles[i].color = (hsv_t){.h = 0, .s = 0, .v = 255};
				} else { // 40% chance of Gold
					twinkles[i].color = (hsv_t){.h = 40, .s = 180, .v = 255};
				}
				break; // Spawn only one new twinkle per frame to avoid
					   // clumping.
			}
		}
	}
}
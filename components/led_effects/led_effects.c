//#include "led_effects.h"
//#include <stdlib.h> // For NULL
//#include <string.h> // For strncpy, although not strictly needed for current param init
//
//// --- Parameters for "Static Color" effect ---
//// This effect displays a single, uniform color across all LEDs.
//// Parameters:
//// - Hue: Defines the color (0-359).
//// - Saturation: Defines the color's intensity (0-255).
//// Note: Brightness (Value) is typically controlled globally by led_controller_set_brightness(),
//// so individual pixel V is set to max (255) and scaled by global brightness.
//static effect_param_t static_color_params[] = {
//    {.name = "Hue", .value = 0, .min = 0, .max = 359, .step = 1},        // Initial value: Red
//    {.name = "Saturation", .value = 255, .min = 0, .max = 255, .step = 1} // Initial value: Full saturation
//};
//
//// --- Parameters for "Candle" effect ---
//// This effect simulates a flickering candle. (Actual flicker logic is TBD in FSM/renderer)
//// Parameters:
//// - Base Hue: The primary color of the candle flame (e.g., orange, yellow).
//// - Flicker Spd: How fast the candle flickers. (Higher value = faster flicker - interpretation TBD).
//// - Intensity: The maximum brightness/value variation during flicker.
//static effect_param_t candle_effect_params[] = {
//    {.name = "Base Hue", .value = 30, .min = 0, .max = 359, .step = 1},     // Initial value: Orange
//    {.name = "Flicker Spd", .value = 50, .min = 10, .max = 100, .step = 5}, // Initial value: Medium speed
//    {.name = "Intensity", .value = 200, .min = 0, .max = 255, .step = 5}   // Initial value: Strong intensity
//};
//
//// --- Array of all defined LED effects ---
//// This array holds the definitions for all effects available in the system.
//// Each effect has a unique ID, a name, and a set of default parameters.
//// The order here determines the default order in selection modes if not sorted otherwise.
//static const led_effect_t all_effects[] = {
//    {
//        .name = "Static Color",
//        .id = 0, // Unique ID for "Static Color"
//        .param_count = sizeof(static_color_params) / sizeof(effect_param_t),
//        .params = static_color_params // Pointer to its default parameters
//    },
//    {
//        .name = "Candle",
//        .id = 1, // Unique ID for "Candle"
//        .param_count = sizeof(candle_effect_params) / sizeof(effect_param_t),
//        .params = candle_effect_params // Pointer to its default parameters
//    }
//    // To add more effects:
//    // 1. Define its parameters array (e.g., `my_new_effect_params[]`).
//    // 2. Add a new `led_effect_t` entry here with a unique ID.
//};
//
//// Total number of defined effects, calculated automatically from the size of the all_effects array.
//static const uint8_t num_effects = sizeof(all_effects) / sizeof(led_effect_t);
//
//// --- API Function Implementations ---
//// Doxygen comments for these functions are in the header file (led_effects.h).
//
///**
// * @brief Get a pointer to the array of all defined LED effects.
// */
//const led_effect_t* led_effects_get_all(uint8_t* count) {
//    if (count == NULL) {
//        return NULL;
//    }
//    *count = num_effects;
//    return all_effects; // Returns a pointer to the first element of the array
//}
//
///**
// * @brief Get a specific LED effect by its ID.
// */
//const led_effect_t* led_effects_get_by_id(uint8_t id) {
//    for (uint8_t i = 0; i < num_effects; i++) {
//        if (all_effects[i].id == id) {
//            return &all_effects[i];
//        }
//    }
//    return NULL; // Not found
//}

//#ifndef LED_EFFECTS_H
//#define LED_EFFECTS_H
//
//#include <stdint.h>
//// #include "cJSON.h" // Omitting for now as per instructions
//
//#ifdef __cplusplus
//extern "C" {
//#endif
//
///**
// * @brief Defines a configurable parameter for an LED effect.
// *
// * This structure allows an effect to expose parameters that can be adjusted at runtime,
// * for example, by user input via an encoder or through a web interface.
// */
//typedef struct {
//    char name[20];          ///< User-friendly name of the parameter (e.g., "Speed", "Hue", "Intensity"). Limited to 19 chars + null terminator.
//    int32_t value;          ///< Current value of the parameter. This is the mutable part that gets updated.
//    int32_t min;            ///< Minimum allowed value for this parameter.
//    int32_t max;            ///< Maximum allowed value for this parameter.
//    int32_t step;           ///< Increment/decrement step when adjusting the value.
//    // char unit[5];        ///< (Optional) Unit of the parameter (e.g., "ms", "%", "deg"). Not currently used.
//} effect_param_t;
//
///**
// * @brief Defines an LED effect, including its properties and parameters.
// *
// * This structure represents a single LED effect available in the system.
// * Effects are typically defined statically in `led_effects.c`.
// */
//typedef struct {
//    const char* name;       ///< User-friendly name of the effect (e.g., "Static Color", "Rainbow Cycle").
//    const uint8_t id;       ///< Unique numerical identifier for the effect.
//    const uint8_t param_count; ///< Number of configurable parameters this effect has.
//    effect_param_t* params; ///< Pointer to an array of `effect_param_t` structures. These are the default parameters.
//                            ///< Note: For runtime modifications, a copy of these parameters should be made.
//    // --- Optional function pointers for more complex effects ---
//    // void (*init_function)(void* effect_runtime_data);
//    //      Called once when the effect is selected or system starts.
//    //      Can be used to initialize per-effect runtime data.
//    // void (*run_function)(void* effect_runtime_data, uint32_t time_ms, led_strip_handle_t strip, uint32_t num_leds);
//    //      Called periodically to update the LED strip for dynamic effects.
//    //      `time_ms` can be used for animations.
//    // void (*deinit_function)(void* effect_runtime_data);
//    //      Called when the effect is deselected or system stops.
//    //      Can be used to free any allocated runtime data.
//    // void* runtime_data; // Pointer to effect-specific runtime data, managed by init/deinit.
//} led_effect_t;
//
///**
// * @brief Retrieves a pointer to the internal array of all defined LED effects.
// *
// * This function provides access to the list of available LED effects. The returned array
// * and its contents should be treated as read-only, as they point to static definitions.
// *
// * @param[out] count Pointer to a `uint8_t` variable where the total number of defined effects will be stored.
// *                   Cannot be NULL.
// * @return Pointer to the first element of the `led_effect_t` array.
// *         If `count` is NULL, or if no effects are defined (though unlikely if module is used),
// *         it might return NULL (or `count` would be 0).
// *         The FSM uses this to populate its list of selectable effects.
// */
//const led_effect_t* led_effects_get_all(uint8_t* count);
//
///**
// * @brief Retrieves a specific LED effect definition by its unique ID.
// *
// * This allows fetching an effect's properties and default parameters using its ID.
// *
// * @param id The unique ID of the effect to retrieve.
// * @return Pointer to the `const led_effect_t` structure if an effect with the given ID is found.
// *         Returns `NULL` if no effect with the specified ID exists.
// */
//const led_effect_t* led_effects_get_by_id(uint8_t id);
//
//#ifdef __cplusplus
//}
//#endif
//
//#endif // LED_EFFECTS_H

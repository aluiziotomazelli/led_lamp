#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "led_effects.h"
#include "nvs_data.h"

/**
 * @brief Structure for the abstract LED output data.
 *
 * This is the data packet that the led_controller sends to the output queue.
 * A downstream driver component would receive this and write the colors to the
 * physical LED strip.
 */
typedef struct {
    color_t *pixels;      // Pointer to the buffer of pixel data
    uint16_t num_pixels;  // Number of pixels in the buffer
    color_mode_t mode;    // The color mode of the pixel data (RGB or HSV)
} led_strip_t;

/**
 * @brief Initialize the LED Controller component.
 *
 * This function creates the LED controller task, initializes the effects engine,
 * and sets up the necessary queues. The controller will listen for commands
 * on the `cmd_queue`.
 *
 * @param cmd_queue The queue for receiving `led_command_t` from the FSM.
 * @return QueueHandle_t The handle to the output queue, which will contain
 *         `led_strip_t` data for the LED driver. Returns NULL on failure.
 */
QueueHandle_t led_controller_init(QueueHandle_t cmd_queue);

/**
 * @brief Applies data loaded from NVS to the LED controller's state.
 *
 * This function should be called at startup after initializing the controller
 * and loading data from NVS.
 *
 * @param v_data Pointer to the loaded volatile data.
 * @param s_data Pointer to the loaded static data.
 */
void led_controller_apply_nvs_data(const volatile_data_t *v_data, const static_data_t *s_data);


// --- State Getter Functions ---

/** @brief Gets the power state of the LEDs. */
bool led_controller_is_on(void);

/** @brief Gets the current master brightness. */
uint8_t led_controller_get_brightness(void);

/** @brief Gets the index of the currently active effect. */
uint8_t led_controller_get_effect_index(void);

/** @brief Gets a pointer to the parameters of the current effect. */
effect_param_t* led_controller_get_effect_params(uint8_t *num_params);


// --- State Modifier Functions ---

/**
 * @brief Increments or decrements the brightness.
 * @param steps The number of steps to change the brightness by.
 * @param limit_hit Pointer to a boolean that will be set to true if a limit was hit.
 * @return The new absolute brightness value.
 */
uint8_t led_controller_inc_brightness(int16_t steps, bool *limit_hit);

/**
 * @brief Increments or decrements the effect index.
 * @param steps The number of steps to change the effect by.
 * @return The new absolute effect index.
 */
uint8_t led_controller_inc_effect(int16_t steps);

/**
 * @brief Increments or decrements the current effect parameter.
 * @param steps The number of steps to change the parameter by.
 * @param limit_hit Pointer to a boolean that will be set to true if a limit was hit.
 * @return The new absolute parameter value, packed as (index << 8 | value).
 */
uint16_t led_controller_inc_effect_param(int16_t steps, bool *limit_hit);

// --- System Setup Functions ---

/** @brief Enters the system setup mode, saving current state. */
void led_controller_enter_system_setup(void);

/** @brief Selects the next system parameter to be edited. */
void led_controller_next_system_param(void);

/**
 * @brief Increments or decrements the current system parameter.
 * @param steps The number of steps to change the parameter by.
 * @param limit_hit Pointer to a boolean that will be set to true if a limit was hit.
 */
void led_controller_inc_system_param(int16_t steps, bool *limit_hit);

/** @brief Saves the temporary system settings. */
void led_controller_save_system_config(void);

/** @brief Cancels any changes to system settings. */
void led_controller_cancel_system_config(void);

/** @brief Restores the current effect's parameters to their default values. */
void led_controller_restore_current_effect_defaults(void);

/** @brief Restores all system and effect parameters to their factory defaults. */
void led_controller_factory_reset(void);

/**
 * @file fsm.h
 * @brief Finite State Machine for LED control system
 * @author Your Name
 * @version 1.0
 */

#pragma once

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// ESP-IDF system
#include "esp_err.h"

// System includes
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief FSM state enumeration
 */
typedef enum {
    MODE_OFF,           ///< System is turned off
    MODE_DISPLAY,       ///< Normal display mode
    MODE_EFFECT_SELECT, ///< Effect selection mode
    MODE_EFFECT_SETUP,  ///< Effect parameter setup mode
    MODE_SYSTEM_SETUP,  ///< System configuration mode
    MODE_OTA,           ///< OTA update mode
} fsm_state_t;

/**
 * @brief LED command types
 */
typedef enum {
    LED_CMD_TURN_OFF,        	///< Turn LEDs off
    LED_CMD_TURN_ON,         	///< Turn LEDs on with fade
    LED_CMD_SET_EFFECT,      	///< Set current effect directly (value = effect_index)
    LED_CMD_SET_BRIGHTNESS,     ///< Set brightness directly (value = brightness)
    LED_CMD_SET_EFFECT_PARAM,   ///< Set an effect parameter directly (value = param_val, param_idx = index)
    LED_CMD_NEXT_EFFECT_PARAM, 	///< Go to next effect parameter (value = step)
    LED_CMD_INC_SYSTEM_PARAM,	///< Increment system parameter (value = step)
    LED_CMD_NEXT_SYSTEM_PARAM,	///< Go to next system parameter (value = step)
    LED_CMD_SAVE_CONFIG,      	///< Save current configuration locally
    LED_CMD_SYNC_AND_SAVE_STATIC_CONFIG, ///< Save static config and sync to slaves
    LED_CMD_CANCEL_CONFIG,      	///< Cancel current configuration
    LED_CMD_ENTER_EFFECT_SETUP,    ///< Enter effect setup mode
    LED_CMD_ENTER_EFFECT_SELECT,   ///< Enter effect selection mode
    LED_CMD_SET_STRIP_MODE,        ///< Set the strip mode (0: Full, 1: Center)

    // --- Feedback Commands ---
    LED_CMD_FEEDBACK_GREEN,        ///< Play a green confirmation blink
    LED_CMD_FEEDBACK_RED,          ///< Play a red cancellation blink
    LED_CMD_FEEDBACK_BLUE,         ///< Play a blue info blink
    LED_CMD_FEEDBACK_EFFECT_COLOR, ///< Play a blink with the effect's base color
    LED_CMD_FEEDBACK_LIMIT,        ///< Play a blink indicating a parameter limit was hit

    LED_CMD_BUTTON_ERROR           ///< Cancel current configuration
} led_cmd_type_t;

/**
 * @brief LED command structure
 */
typedef struct {
    led_cmd_type_t cmd;     ///< Command type
    uint64_t timestamp;     ///< Timestamp when command was generated
    int16_t value;          ///< Step value or fixed value
    uint8_t param_idx;      ///< Index of the parameter to modify
} led_command_t;

/**
 * @brief Initialize the FSM module
 * 
 * @param[in] inputQueue Queue for integrated input events (integrated_event_t)
 * @param[in] outputQueue Queue for LED commands (led_command_t)
 * 
 * @note This function must be called before any other FSM functions
 * @warning Queues must be properly initialized before calling this function
 */
void fsm_init(QueueHandle_t inputQueue, QueueHandle_t outputQueue);

/**
 * @brief Get current FSM state
 * 
 * @return Current FSM state
 * 
 * @note Useful for debugging and monitoring system state
 */
fsm_state_t fsm_get_state(void);

/**
 * @brief Sets the initial state of the FSM
 *
 * This should be called once at startup, after fsm_init() and after
 * loading the desired state from a persistent source.
 *
 * @param[in] state The state to set as the initial state
 * 
 * @note This function resets the timeout timer for the new state
 */
void fsm_set_initial_state(fsm_state_t state);
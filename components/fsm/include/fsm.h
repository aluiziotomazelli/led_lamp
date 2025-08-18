#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
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
    MODE_SYSTEM_SETUP,   ///< System configuration mode
} fsm_state_t;



/**
 * @brief LED command types
 */
typedef enum {
    LED_CMD_TURN_OFF,        	///< Turn LEDs off
    LED_CMD_TURN_ON,         	///< Turn LEDs on
    LED_CMD_TURN_ON_FADE,         	///< Turn LEDs on
    LED_CMD_SET_EFFECT,      	///< Set current effect directly (value = effect_index)
    LED_CMD_SET_BRIGHTNESS,     ///< Set brightness directly (value = brightness)
    LED_CMD_SET_EFFECT_PARAM,   ///< Set an effect parameter directly (value = (param_idx << 8) | param_val)
    LED_CMD_NEXT_EFFECT_PARAM, 	///< Go to next effect parameter (value = step)
    LED_CMD_INC_SYSTEM_PARAM,	///< Increment system parameter (value = step)
    LED_CMD_NEXT_SYSTEM_PARAM,	///< Go to next system parameter (value = step)
    LED_CMD_SAVE_CONFIG,      	///< Save current configuration
    LED_CMD_CANCEL_CONFIG,      	///< Cancel current configuration
    LED_CMD_ENTER_EFFECT_SETUP,
    LED_CMD_ENTER_EFFECT_SELECT,
    LED_CMD_SET_STRIP_MODE,     ///< Set the strip mode (0: Full, 1: Center)

    // --- Feedback Commands ---
    LED_CMD_FEEDBACK_GREEN,     ///< Play a green confirmation blink
    LED_CMD_FEEDBACK_RED,       ///< Play a red cancellation blink
    LED_CMD_FEEDBACK_BLUE,      ///< Play a blue info blink
    LED_CMD_FEEDBACK_EFFECT_COLOR, ///< Play a blink with the effect's base color

    LED_CMD_BUTTON_ERROR      	///< Cancel current configuration
    
} led_cmd_type_t;

/**
 * @brief LED command structure
 */
typedef struct {
    led_cmd_type_t cmd;     ///< Command type
    uint64_t timestamp;
    int16_t value;          ///< Step value or fixed value
} led_command_t;

/**
 * @brief Initialize the FSM module
 * @param inputQueue Queue for integrated input events (integrated_event_t)
 * @param outputQueue Queue for LED commands (led_command_t)
 */
void fsm_init(QueueHandle_t inputQueue, QueueHandle_t outputQueue);

/**
 * @brief Get current FSM state
 * @return Current FSM state
 */
fsm_state_t fsm_get_state(void);


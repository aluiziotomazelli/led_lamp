/**
 * @file button.h
 * @brief Button driver with multi-click detection support
 * @author Your Name
 * @version 1.0
 */

#pragma once

// System includes
#include <stdint.h>
#include <stdbool.h>

// ESP-IDF drivers
#include "driver/gpio.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Forward declaration of button handle (opaque type)
typedef struct button_s button_t;

/**
 * @brief Button click type enumeration
 */
typedef enum {
    BUTTON_NONE_CLICK,          ///< No click detected
    BUTTON_CLICK,               ///< Single click
    BUTTON_DOUBLE_CLICK,        ///< Double click
    BUTTON_LONG_CLICK,          ///< Long press (1+ seconds)
    BUTTON_VERY_LONG_CLICK,     ///< Very long press (3+ seconds)
    BUTTON_TIMEOUT,             ///< Press timeout
    BUTTON_ERROR                ///< Error state
} button_click_type_t;

/**
 * @brief Button event structure
 */
typedef struct {
    button_click_type_t type;   ///< Type of detected click
    gpio_num_t pin;             ///< Source GPIO pin
} button_event_t;

/**
 * @brief Button configuration structure
 */
typedef struct {
    gpio_num_t pin;                 ///< GPIO pin number
    bool active_low;                ///< True if pressed state is LOW
    uint16_t debounce_press_ms;     ///< Press debounce time (milliseconds)
    uint16_t debounce_release_ms;   ///< Release debounce time (milliseconds)
    uint16_t double_click_ms;       ///< Max interval between double clicks (ms)
    uint16_t long_click_ms;         ///< Minimum duration for long click (ms)
    uint16_t very_long_click_ms;    ///< Minimum duration for very long click (ms)
} button_config_t;

/**
 * @brief Create a new button instance
 * 
 * @param[in] config Button configuration parameters
 * @param[in] output_queue Queue for button events
 * @return button_t* Button handle, NULL on error
 * 
 * @note The queue must be created beforehand with adequate size
 * @warning Do not call this function from an ISR
 */
button_t *button_create(const button_config_t* config, QueueHandle_t output_queue);

/**
 * @brief Delete a button instance and free resources
 * 
 * @param[in] btn Button handle to delete
 * 
 * @note This function does not delete the event queue
 */
void button_delete(button_t *btn);

/**
 * @brief Resets the internal state machine of the button.
 *
 * @details This function should be called when the button's context might
 * become invalid, such as before entering a sleep mode where timers do not run.
 * It resets the state to BUTTON_WAIT_FOR_PRESS and clears any pending click logic.
 *
 * @param btn Button handle to reset.
 */
void button_reset_state(button_t *btn);
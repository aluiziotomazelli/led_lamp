#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief Enumeration of button click types
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
 * @brief Structure representing a button event
 */
typedef struct {
    button_click_type_t type;   ///< Type of detected click
    gpio_num_t pin;             ///< Source GPIO pin
} button_event_t;

// Forward declaration of button handle
typedef struct button_s button_t;

/**
 * @brief Button configuration structure
 */
typedef struct {
    gpio_num_t pin;             	///< GPIO pin number
    bool active_low;            	///< True if pressed state is LOW
    uint16_t debounce_press_ms; 	///< Press debounce time (milliseconds)
    uint16_t debounce_release_ms; 	///< Release debounce time (milliseconds)
    uint16_t double_click_ms;   	///< Max interval between double clicks (ms)
    uint16_t long_click_ms;     	///< Minimum duration for long click (ms)
    uint16_t very_long_click_ms; 	///< Minimum duration for very long click (ms)
} button_config_t;

/**
 * @brief Create a new button instance
 * @param config Button configuration parameters
 * @param output_queue Queue for button events
 * @return button_t* Button handle, NULL on error
 */
button_t *button_create(const button_config_t* config, QueueHandle_t output_queue);

/**
 * @brief Delete a button instance and free resources
 * @param btn Button handle to delete
 */
void button_delete(button_t *btn);
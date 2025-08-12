#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/touch_pad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief Enumeration of possible touch events
 */
typedef enum {
    TOUCH_NONE,     ///< No touch event detected
    TOUCH_PRESS,    ///< Short press event
    TOUCH_HOLD,     ///< Long press/hold event
    TOUCH_ERROR     ///< Error condition
} touch_event_type_t;

/**
 * @brief Structure representing a touch event
 */
typedef struct {
    touch_event_type_t type;  ///< Type of touch event
    touch_pad_t pad;          ///< GPIO pad number that generated the event
} touch_event_t;

/**
 * @brief Configuration structure for touch button initialization
 */
typedef struct {
    touch_pad_t pad;                     ///< GPIO pad number to use
    uint16_t threshold_percent;          ///< Activation threshold as percentage of baseline (e.g., 20 = 20%)
    uint16_t debounce_press_ms;          ///< Debounce time for press events (milliseconds)
    uint16_t debounce_release_ms;        ///< Debounce time for release events (milliseconds)
    uint16_t hold_time_ms;               ///< Time required to trigger hold event (milliseconds)
    bool enable_hold_repeat;             ///< Enable repeated hold events while pressed
    uint_fast16_t hold_repeat_interval_ms; ///< Interval between repeated hold events (milliseconds)
    uint16_t recalibration_interval_min; ///< Automatic recalibration interval (minutes)
} touch_config_t;

// Forward declaration of touch handle structure
typedef struct touch_s touch_t;

/**
 * @brief Create a new touch button instance
 * @param config Pointer to configuration structure
 * @param output_queue FreeRTOS queue for event output
 * @return Pointer to new touch instance, NULL on failure
 */
touch_t* touch_create(const touch_config_t* config, QueueHandle_t output_queue);

/**
 * @brief Delete a touch button instance and free resources
 * @param touch_handle Pointer to touch instance to delete
 */
void touch_delete(touch_t* touch_handle);
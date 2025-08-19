/**
 * @file switch.h
 * @brief Switch driver with debouncing support
 * @author Your Name
 * @version 1.0
 */

#pragma once

// System includes
#include <stdbool.h>
#include <stdint.h>

// ESP-IDF drivers
#include "driver/gpio.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/**
 * @brief Opaque pointer to a switch instance
 */
typedef struct switch_s *switch_t;

/**
 * @brief Switch configuration structure
 */
typedef struct {
    gpio_num_t pin;       ///< GPIO pin for the switch
    bool active_low;      ///< True if the switch is active low (connects to GND when closed)
    uint16_t debounce_ms; ///< Debounce time in milliseconds
} switch_config_t;

/**
 * @brief Switch event data structure, sent to the queue
 */
typedef struct {
    gpio_num_t pin;  ///< The GPIO pin number of the switch that triggered the event
    bool is_closed;  ///< true if the switch is in the "closed" position, false if "open"
} switch_event_t;

/**
 * @brief Create a new switch instance
 *
 * @param[in] config Pointer to the switch configuration structure
 * @param[in] queue The queue to which switch events will be sent
 * @return switch_t Handle to the created switch instance, or NULL on failure
 * 
 * @note The output queue must be created before calling this function
 * @warning GPIO pins must be configured with appropriate pull resistors
 */
switch_t switch_create(const switch_config_t *config, QueueHandle_t queue);

/**
 * @brief Delete a switch instance and free its resources
 *
 * @param[in] sw The switch handle to delete
 * 
 * @note This function does not delete the event queue
 * @warning Ensure no tasks are using the switch before deletion
 */
void switch_delete(switch_t sw);
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/** @file button.h
 *  @brief Interface for a configurable button component.
 *
 *  This component provides functionality to detect various types of button
 *  clicks (single, double, long, very long) with debouncing. It uses FreeRTOS
 *  tasks and queues for event handling.
 */

// Definições padrão (pode ajustar)
/** @def DEBOUNCE_PRESS_MS
 *  @brief Default debounce time in milliseconds after a button press is detected.
 *  Prevents multiple triggers from a single physical press due to contact bounce.
 */
#define DEBOUNCE_PRESS_MS 40

/** @def DEBOUNCE_RELEASE_MS
 *  @brief Default debounce time in milliseconds after a button release is detected.
 *  Prevents multiple triggers from a single physical release.
 */
#define DEBOUNCE_RELEASE_MS 60

/** @def DOUBLE_CLICK_MS
 *  @brief Default maximum time in milliseconds between two clicks to be considered a double click.
 */
#define DOUBLE_CLICK_MS 200

/** @def LONG_CLICK_MS
 *  @brief Default minimum time in milliseconds a button must be held pressed to be considered a long click.
 */
#define LONG_CLICK_MS 1000

/** @def VERY_LONG_CLICK_MS
 *  @brief Default minimum time in milliseconds a button must be held pressed to be considered a very long click.
 */
#define VERY_LONG_CLICK_MS 3000

/**
 * @brief Defines the types of button click events that can be detected.
 */
typedef enum {
    BUTTON_NONE_CLICK,          ///< No click event detected or recognized yet. This is often an intermediate state.
    BUTTON_CLICK,               ///< A single, short press and release.
    BUTTON_DOUBLE_CLICK,        ///< Two single clicks detected within the `DOUBLE_CLICK_MS` window.
    BUTTON_LONG_CLICK,          ///< Button held pressed for at least `LONG_CLICK_MS` but less than `VERY_LONG_CLICK_MS`.
    BUTTON_VERY_LONG_CLICK,     ///< Button held pressed for at least `VERY_LONG_CLICK_MS`.
    BUTTON_TIMEOUT,             ///< Indicates a timeout occurred during the button press sequence, e.g., pressed for too long without release for certain states.
    BUTTON_ERROR                ///< Indicates an error state in the button processing logic, e.g. unexpected sequence.
} button_click_type_t;

/**
 * @brief Structure to hold button event data.
 *
 * This structure is used to pass button event information via a FreeRTOS queue.
 */
typedef struct {
    button_click_type_t type;   ///< The type of click event that was detected (e.g., BUTTON_CLICK, BUTTON_LONG_CLICK).
} button_event_t;

/**
 * @brief Opaque type definition for a button instance.
 *
 * This declaration hides the internal structure (`struct button_s`) of a button object.
 * Users interact with button instances through pointers to this opaque type,
 * promoting encapsulation and cleaner APIs. The actual structure is defined
 * privately in `button.c`.
 */
typedef struct button_s button_t;

/**
 * @brief Creates and initializes a new button instance.
 *
 * This function configures the specified GPIO pin as an input with a pull-up resistor,
 * sets up an ISR for button presses, creates a FreeRTOS task to handle button logic
 * (debouncing, click type detection), and a queue to send button events.
 *
 * @param pin The GPIO number to which the button is connected.
 * @return A pointer to the created button_t instance, or NULL if an error occurred
 *         (e.g., memory allocation failure, task creation failure).
 */
button_t *button_create(gpio_num_t pin);

/**
 * @brief Retrieves the event queue for a button instance.
 *
 * This queue can be used with `xQueueReceive` (or other FreeRTOS queue functions)
 * to wait for and receive button events from the button task. Each event is of
 * type `button_event_t`.
 *
 * @param btn A pointer to the button_t instance.
 * @return A `QueueHandle_t` for the button's event queue, or NULL if the button instance is invalid.
 */
QueueHandle_t button_get_event_queue(button_t *btn);

/**
 * @brief Sets custom debounce times for a button instance.
 *
 * Allows overriding the default debounce timings.
 *
 * @param btn A pointer to the button_t instance.
 * @param debounce_press_ms Debounce time in milliseconds after a press is detected.
 * @param debounce_release_ms Debounce time in milliseconds after a release is detected.
 */
void button_set_debounce(button_t *btn, uint16_t debounce_press_ms, uint16_t debounce_release_ms);

/**
 * @brief Sets custom click detection times for a button instance.
 *
 * Allows overriding the default timings that define double clicks, long clicks,
 * and very long clicks.
 *
 * @param btn A pointer to the button_t instance.
 * @param double_click_ms Maximum time in milliseconds between two clicks for a double click.
 * @param long_click_ms Minimum press duration in milliseconds for a long click.
 * @param very_long_click_ms Minimum press duration in milliseconds for a very long click.
 */
void button_set_click_times(button_t *btn, uint16_t double_click_ms, uint16_t long_click_ms, uint16_t very_long_click_ms);

/**
 * @brief Deletes a button instance and frees associated resources.
 *
 * This function stops the button task, deletes the event queue, and frees the memory
 * allocated for the button_t instance. It's important to call this for any button
 * created with `button_create` when it's no longer needed to prevent resource leaks.
 *
 * @param btn A pointer to the button_t instance to be deleted.
 */
void button_delete(button_t *btn);

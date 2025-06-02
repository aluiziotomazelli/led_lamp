#include "button.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"

// Sets the local log level to DEBUG, allowing ESP_LOGD messages.
// Useful for detailed debugging of the component's behavior.
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
// TAG used to identify log messages from this component.
static const char *TAG = "Button";

/**
 * @brief Enumeration of the internal states of the button state machine.
 *
 * Defines the different stages the button goes through when detecting clicks.
 */
typedef enum {
	BUTTON_WAIT_FOR_PRESS,            ///< Initial state, waiting for the button to be pressed.
	BUTTON_DEBOUNCE_PRESS,            ///< Waiting for debounce to finish after the button is pressed.
	BUTTON_WAIT_FOR_RELEASE,          ///< Button pressed and debounce complete, waiting for it to be released.
	BUTTON_DEBOUNCE_RELEASE,          ///< Waiting for debounce to finish after the button is released.
	BUTTON_WAIT_FOR_DOUBLE,           ///< Button released (after a click), waiting for a possible second click to characterize a double click.
	BUTTON_TIMEOUT_WAIT_FOR_RELEASE   ///< Button pressed for an excessive time (timeout), waiting for it to be released.
} button_state_t;

/**
 * @brief Internal structure that stores the state and configuration of a button instance.
 *
 * Contains all the necessary information for the operation of an individual button,
 * including GPIO pin, current state of the state machine, timings,
 * and handles for the associated FreeRTOS task and queue.
 */
struct button_s {
	gpio_num_t pin;                   ///< GPIO pin number to which the button is connected.
	button_state_t state;             ///< Current state of the button's state machine.
	uint32_t last_time_ms;            ///< Time in milliseconds of the last state transition or relevant event.
	uint32_t press_start_time_ms;     ///< Time in milliseconds when the button was first pressed.
	bool first_click;                 ///< Flag for double-click logic control, indicates if the first click of a potential double click has occurred.

	// Configurable times (in milliseconds)
	uint32_t debounce_press_ms;       ///< Debounce time for button press.
	uint32_t debounce_release_ms;     ///< Debounce time for button release.
	uint32_t double_click_ms;         ///< Maximum time window to detect a double click.
	uint32_t long_click_ms;           ///< Minimum duration for a long click.
	uint32_t very_long_click_ms;      ///< Minimum duration for a very long click.
	uint32_t timeout_ms;              ///< Maximum time the button can be pressed before a timeout event.

	QueueHandle_t queue;              ///< Handle of the FreeRTOS queue to send detected click events.
	TaskHandle_t task_handle;         ///< Handle of the FreeRTOS task that manages this button.
};

/**
 * @brief Gets the current system time in milliseconds.
 *
 * Utility function that encapsulates `esp_timer_get_time()` to return the time in ms.
 * @return The current time in milliseconds.
 */
static uint32_t get_current_time_ms() { return esp_timer_get_time() / 1000; }

/**
 * @brief Main state machine function that detects the type of click.
 *
 * Evaluates the current state of the button, the GPIO level, and timings to
 * determine if a click event has occurred (single, double, long, etc.).
 * This function is called repeatedly by the button task.
 *
 * @param btn Pointer to the button structure (`button_t`) to be evaluated.
 * @return The type of click detected (`button_click_type_t`). Returns `BUTTON_NONE_CLICK`
 *         if no complete click event was detected in this call.
 */
static button_click_type_t button_get_click(button_t *btn) {
	uint32_t now = get_current_time_ms(); // Current time for duration calculations

	switch (btn->state) {
	case BUTTON_WAIT_FOR_PRESS:
		// Waiting for the button to be pressed (logic level 0)
		if (gpio_get_level(btn->pin) == 0) {
			btn->press_start_time_ms = now; // Record the time of pressing
			btn->state = BUTTON_DEBOUNCE_PRESS; // Change to debounce press state
			ESP_LOGD("FSM", "Pin %d: Pressed, starting debounce", btn->pin);
		}
		break;

	case BUTTON_DEBOUNCE_PRESS:
		// Waiting for the press debounce time to pass
		if (now - btn->press_start_time_ms > btn->debounce_press_ms) {
			btn->state = BUTTON_WAIT_FOR_RELEASE; // Debounce complete, wait for release
			ESP_LOGD("FSM", "Pin %d: Press debounce OK, waiting for release", btn->pin);
		}
		break;

	case BUTTON_WAIT_FOR_RELEASE:
		// Button is pressed, waiting to be released (logic level 1)
		if (gpio_get_level(btn->pin) == 1) {
			uint32_t duration = now - btn->press_start_time_ms; // Calculate press duration

			// Check if it was a long or very long click
			if (duration > btn->very_long_click_ms) {
				btn->state = BUTTON_WAIT_FOR_PRESS; // Return to initial state
				return BUTTON_VERY_LONG_CLICK;
			} else if (duration > btn->long_click_ms) {
				btn->state = BUTTON_WAIT_FOR_PRESS; // Return to initial state
				return BUTTON_LONG_CLICK;
			} else {
				// It was a short click, record time and wait for release debounce
				btn->last_time_ms = now;
                btn->state = BUTTON_DEBOUNCE_RELEASE;
				ESP_LOGD("FSM", "Pin %d: Released, starting release debounce (duration: %dms)", btn->pin, duration);
			}
		} else if (now - btn->press_start_time_ms > btn->timeout_ms) {
			// Button pressed for too long (timeout)
			btn->state = BUTTON_TIMEOUT_WAIT_FOR_RELEASE; // Change to timeout state
			btn->last_time_ms = now; // Record timeout time
			ESP_LOGD("FSM", "Pin %d: Timeout while pressed, waiting for release", btn->pin);
			// The BUTTON_TIMEOUT event will be returned when the button is finally released in this state
		}
		break;

	case BUTTON_DEBOUNCE_RELEASE:
		// Waiting for the release debounce time to pass
		if (now - btn->last_time_ms > btn->debounce_release_ms) {
			btn->state = BUTTON_WAIT_FOR_DOUBLE; // Debounce complete, wait for possible double click
			ESP_LOGD("FSM", "Pin %d: Release debounce OK, waiting for double click", btn->pin);
		}
		break;

	case BUTTON_WAIT_FOR_DOUBLE:
		// Waiting for a second press for double click or end of time window for single click
		if (gpio_get_level(btn->pin) == 0 && !btn->first_click) {
			// Detected a new press within the double click window
			btn->last_time_ms = now;
			btn->first_click = true; // Mark that the first click of a double has occurred
			btn->press_start_time_ms = now; // Reset press time for the second click
			btn->state = BUTTON_DEBOUNCE_PRESS; // Return to debounce of the (second) press
			ESP_LOGD("FSM", "Pin %d: Potential double click, second press detected", btn->pin);
		} else if (now - btn->last_time_ms > btn->double_click_ms) {
			// Double click time window expired
			btn->state = BUTTON_WAIT_FOR_PRESS; // Return to initial state
			if (btn->first_click) {
				// If first_click was true, it means the second click was completed (pressed and released)
				// and now the double click window closed AFTER the second click.
				btn->first_click = false;
				ESP_LOGD("FSM", "Pin %d: Double click detected", btn->pin);
				return BUTTON_DOUBLE_CLICK;
			} else {
				// No second click, so it was a single click
				ESP_LOGD("FSM", "Pin %d: Single click detected", btn->pin);
				return BUTTON_CLICK;
			}
		}
		break;

	case BUTTON_TIMEOUT_WAIT_FOR_RELEASE:
		// Button was in timeout and has now been released
		if (gpio_get_level(btn->pin) == 1) {
			// Wait for release debounce after timeout
			if (now - btn->last_time_ms > btn->debounce_release_ms) {
                btn->last_time_ms = now;
                btn->state = BUTTON_WAIT_FOR_PRESS; // Return to initial state
			    ESP_LOGD("FSM", "Pin %d: Released after timeout, release debounce OK", btn->pin);
				return BUTTON_TIMEOUT; // Return timeout event
			}
		} else {
			// Button remains pressed even after initial timeout.
			// This could indicate an error or a stuck button.
			btn->last_time_ms = now; // Update time to prevent premature error detection
			if (now - btn->press_start_time_ms > 2 * btn->timeout_ms) { // An additional timeout period
				btn->state = BUTTON_WAIT_FOR_PRESS; // Reset to initial state
			    ESP_LOGD("FSM", "Pin %d: Error, button still pressed long after timeout", btn->pin);
				return BUTTON_ERROR; // Return an error
			}
		}
		break;
	}

	return BUTTON_NONE_CLICK; // No complete click event detected in this call
}

/**
 * @brief Dedicated FreeRTOS task for managing a button.
 *
 * This task waits for notifications from the button's ISR (indicating a change on the GPIO pin).
 * When notified, it temporarily disables the pin's interrupt,
 * executes the state machine (`button_get_click`) to detect the click type,
 * sends the event to the queue, and then re-enables the interrupt.
 *
 * @param param Pointer to the `button_t` structure associated with this task.
 */
static void button_task(void *param) {
	button_t *btn = (button_t *)param; // Cast the parameter to the correct type
	button_event_t event;              // Structure to store the event to be sent to the queue
	bool processing = false;           // Flag to control if processing is active

	while (1) {
		// Wait for a notification from the ISR. Blocks indefinitely until notified.
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // pdTRUE to clear the notification value upon receipt

		if (!processing) {
			processing = true; // Mark that processing has started
			gpio_intr_disable(btn->pin); // Disable interrupts for this pin during processing
			                             // to prevent re-entrancy or race conditions.
			ESP_LOGD(TAG, "Task pin %d: Notified, starting processing", btn->pin);
		}

		// Processing loop: continues as long as there is something to process (states to transition)
		do {
			// Call the state machine to check if a click was detected
			button_click_type_t click = button_get_click(btn);

			if (click != BUTTON_NONE_CLICK) { // If a click type was detected
				event.type = click; // Prepare the event

				// Try to send the event to the queue. Waits a maximum of 50ms if the queue is full.
				if (xQueueSend(btn->queue, &event, pdMS_TO_TICKS(50))) {
					ESP_LOGD(TAG, "Button %d: click %d queued", btn->pin, click);
				} else {
					ESP_LOGW(TAG, "Failed to queue event for button %d: click %d (queue full?)", btn->pin, click);
				}

				// Current event processing finished.
				// Re-enable ISR and exit the internal processing loop.
				gpio_intr_enable(btn->pin);
				processing = false; // Mark that processing has finished
				ESP_LOGD(TAG, "Task pin %d: Processing complete, interrupt re-enabled", btn->pin);
				break; // Exit do-while loop
			}
			// If no click was detected (BUTTON_NONE_CLICK), the state machine might still be
			// in an intermediate state (e.g., debounce). The task yields for a short time (10ms)
			// and then re-evaluates the button state in the next cycle of the do-while loop.
			vTaskDelay(pdMS_TO_TICKS(10));
		} while (processing); // Continue inner loop if 'processing' is still true (usually not after a click)
	}
}

/**
 * @brief Interrupt Service Handler (ISR) for the button pin.
 *
 * This function is executed when an interrupt occurs on the button pin (configured for falling edge/NEGEDGE).
 * Its main responsibility is to notify the `button_task` that button activity has occurred.
 * The ISR should be as short as possible.
 *
 * @param arg Pointer to the `button_t` structure associated with the pin that generated the interrupt.
 */
static void IRAM_ATTR button_isr_handler(void *arg) {
	button_t *btn = (button_t *)arg; // Cast the argument to the correct type

	// Notify the button task that an interrupt occurred.
	// `eNoAction` specifies that no bits in the task's notification value will be changed.
	// The task is only woken up.
	BaseType_t xHigherPriorityTaskWoken = pdFALSE; // Flag to be used by portYIELD_FROM_ISR
	ESP_DRAM_LOGD(TAG, "ISR pin %d: Interrupt detected, notifying task", btn->pin); // Log to DRAM (faster in ISR)
	xTaskNotifyFromISR(btn->task_handle, 0, eNoAction, &xHigherPriorityTaskWoken);

	// If xHigherPriorityTaskWoken is pdTRUE, it means the notification woke up
	// a task of higher priority than the currently running task (the one that was interrupted).
	// In this case, `portYIELD_FROM_ISR` forces an immediate context switch.
	if (xHigherPriorityTaskWoken) {
		portYIELD_FROM_ISR();
	}
}

/**
 * @brief Creates and initializes a new button instance.
 *
 * Allocates memory for the button structure, configures the specified GPIO pin as input
 * with an internal pull-up resistor and falling edge interrupt.
 * Initializes the GPIO ISR service (if not already active), adds the ISR handler
 * for this button, creates the event queue, and the FreeRTOS task to manage the button.
 *
 * @param pin The GPIO number (`gpio_num_t`) to which the button is connected.
 * @return Pointer to the created and initialized `button_t` structure, or `NULL` in case of failure
 *         (e.g., memory allocation failure, task or queue creation failure).
 */
button_t *button_create(gpio_num_t pin) {
	// Allocate memory for the button structure and initialize with zeros.
	button_t *btn = calloc(1, sizeof(button_t));
	if (!btn) {
		ESP_LOGE(TAG, "Failed to allocate memory for button on pin %d", pin);
		return NULL;
	}

	// Initialize the button structure fields.
	btn->pin = pin;
	btn->state = BUTTON_WAIT_FOR_PRESS; // Initial state
    btn->press_start_time_ms = 0;       // Press start time zeroed
    btn->last_time_ms = 0;              // Last event time zeroed
    btn->first_click = false;           // Flag for double click

    // Set default times for debounce and click detection.
    // These can be changed later with `button_set_*` functions.
    btn->debounce_press_ms = DEBOUNCE_PRESS_MS;
	btn->debounce_release_ms = DEBOUNCE_RELEASE_MS;
	btn->double_click_ms = DOUBLE_CLICK_MS;
	btn->long_click_ms = LONG_CLICK_MS;
	btn->very_long_click_ms = VERY_LONG_CLICK_MS;
	btn->timeout_ms = VERY_LONG_CLICK_MS * 2; // Timeout is twice the very long click by default.

	// Create the FreeRTOS queue for button events.
	// The queue can store up to 5 events of type `button_event_t`.
	btn->queue = xQueueCreate(5, sizeof(button_event_t));
	if (!btn->queue) {
		ESP_LOGE(TAG, "Failed to create queue for button on pin %d", pin);
		free(btn); // Free allocated memory for the button structure.
		return NULL;
	}

	// Configure the GPIO pin.
	gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),       // Bitmask for the selected pin.
        .mode = GPIO_MODE_INPUT,             // Input mode.
        .pull_up_en = GPIO_PULLUP_ENABLE,    // Enable internal pull-up resistor.
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // Disable pull-down resistor.
        .intr_type = GPIO_INTR_NEGEDGE       // Interrupt type: falling edge (button press).
    };
	esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", pin, esp_err_to_name(err));
        vQueueDelete(btn->queue);
        free(btn);
        return NULL;
    }
    
	// Install the global ISR service for GPIOs, if not already installed.
	// The '0' argument refers to the interrupt priority level (can be adjusted).
	// This function only needs to be called once in the project, but subsequent calls are safe.
	err = gpio_install_isr_service(0);
    // ESP_INTR_FLAG_LEVEL1 defines a low priority for the interrupt.
    // It might already be installed, so we ignore ESP_ERR_INVALID_STATE.
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
        vQueueDelete(btn->queue);
        free(btn);
        return NULL;
    }


	// Add the ISR handler for the specific button pin.
	// `button_isr_handler` will be called when the interrupt occurs on `pin`.
	// `btn` is passed as an argument to the handler.
	err = gpio_isr_handler_add(pin, button_isr_handler, btn);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for GPIO %d: %s", pin, esp_err_to_name(err));
        vQueueDelete(btn->queue);
        free(btn);
        return NULL;
    }

	// Create the FreeRTOS task that will manage the button logic.
	// "button_task" is the task name (useful for debugging).
	// 2048 is the stack size in words.
	// `btn` is passed as a parameter to the task.
	// 10 is the task priority.
	// `&btn->task_handle` stores the handle of the created task.
	BaseType_t res = xTaskCreate(button_task, "button_task", 2048, btn, 10, &btn->task_handle);
	if (res != pdPASS) {
		ESP_LOGE(TAG, "Failed to create task for button on pin %d", pin);
		gpio_isr_handler_remove(pin); // Remove previously added ISR handler.
		vQueueDelete(btn->queue);     // Delete the queue.
		free(btn);                    // Free memory.
		return NULL;
	}

	ESP_LOGI(TAG, "Button created successfully on pin %d", pin);
	return btn; // Return pointer to the initialized button structure.
}

/**
 * @brief Gets the event queue handle for a specific button.
 *
 * The application can use this handle with `xQueueReceive` to wait for and
 * receive click events from the button.
 *
 * @param btn Pointer to the `button_t` structure of the desired button.
 * @return `QueueHandle_t` of the event queue, or `NULL` if `btn` is invalid.
 */
QueueHandle_t button_get_event_queue(button_t *btn) {
	if (!btn) {
		ESP_LOGW(TAG, "Attempt to get queue from a null button.");
		return NULL;
	}
	return btn->queue;
}

/**
 * @brief Adjusts the debounce times for a specific button.
 *
 * Allows modification of debounce times for button press and release,
 * overriding default values.
 *
 * @param btn Pointer to the `button_t` structure of the button to be configured.
 * @param debounce_press_ms New debounce time (in ms) for pressing.
 * @param debounce_release_ms New debounce time (in ms) for releasing.
 */
void button_set_debounce(button_t *btn, uint16_t debounce_press_ms, uint16_t debounce_release_ms) {
	if (btn) {
		btn->debounce_press_ms = debounce_press_ms;
		btn->debounce_release_ms = debounce_release_ms;
		ESP_LOGI(TAG, "Pin %d: Debounce times adjusted: press %ums, release %ums",
				 btn->pin, debounce_press_ms, debounce_release_ms);
	} else {
		ESP_LOGW(TAG, "Attempt to set debounce for a null button.");
	}
}

/**
 * @brief Adjusts click detection times (double, long, very long) for a button.
 *
 * Allows modification of time parameters that define different click types,
 * overriding default values. The timeout is also recalculated based on
 * `very_long_click_ms`.
 *
 * @param btn Pointer to the `button_t` structure of the button to be configured.
 * @param double_click_ms New maximum time window (in ms) for double click.
 * @param long_click_ms New minimum duration (in ms) for long click.
 * @param very_long_click_ms New minimum duration (in ms) for very long click.
 */
void button_set_click_times(button_t *btn, uint16_t double_click_ms, uint16_t long_click_ms, uint16_t very_long_click_ms) {
	if (btn) {
		btn->double_click_ms = double_click_ms;
		btn->long_click_ms = long_click_ms;
		btn->very_long_click_ms = very_long_click_ms;
		// Recalculate timeout based on the new very long click time.
		btn->timeout_ms = very_long_click_ms * 2;
		ESP_LOGI(TAG, "Pin %d: Click times adjusted: double %ums, long %ums, very long %ums, timeout %ums",
				 btn->pin, double_click_ms, long_click_ms, very_long_click_ms, btn->timeout_ms);
	} else {
		ESP_LOGW(TAG, "Attempt to set click times for a null button.");
	}
}

/**
 * @brief Deletes a button instance and frees all associated resources.
 *
 * Stops the button's FreeRTOS task, removes the GPIO ISR handler, deletes the event queue,
 * and frees the memory allocated for the `button_t` structure.
 * It's crucial to call this function when a button is no longer needed to prevent resource leaks.
 *
 * @param btn Pointer to the `button_t` structure of the button to be deleted.
 */
void button_delete(button_t *btn) {
	if (btn) {
		ESP_LOGI(TAG, "Deleting button on pin %d...", btn->pin);
		// Remove the GPIO ISR handler.
		// It's important to do this before deleting the task, to prevent the ISR from trying to notify a non-existent task.
		esp_err_t err = gpio_isr_handler_remove(btn->pin);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to remove ISR handler for GPIO %d: %s. Continuing with deletion...", btn->pin, esp_err_to_name(err));
        }

		// Delete the button task, if the handle is valid.
		if (btn->task_handle) {
			vTaskDelete(btn->task_handle);
			ESP_LOGD(TAG, "Button task on pin %d deleted.", btn->pin);
			btn->task_handle = NULL; // Invalidate handle to prevent double use.
		} else {
			ESP_LOGW(TAG, "Button task on pin %d already deleted or not initialized.", btn->pin);
		}

		// Delete the button queue, if the handle is valid.
		if (btn->queue) {
			vQueueDelete(btn->queue);
			ESP_LOGD(TAG, "Button queue on pin %d deleted.", btn->pin);
			btn->queue = NULL; // Invalidate handle.
		} else {
			ESP_LOGW(TAG, "Button queue on pin %d already deleted or not initialized.", btn->pin);
		}

		// Free the memory allocated for the button structure.
		free(btn);
		ESP_LOGI(TAG, "Button on pin %d deleted successfully.", btn->pin); // Uses pin value before btn is freed if log was after.
	} else {
		ESP_LOGW(TAG, "Attempt to delete a null button.");
	}
}

/**
 * @file button.c
 * @brief Button driver implementation with state machine for click detection
 * 
 * @details This file implements a finite state machine for detecting single, double,
 * long, and very long button clicks with debouncing for both press and release events.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <stdint.h>
#include <stdbool.h>

// ESP-IDF drivers
#include "driver/gpio.h"
#include "hal/gpio_types.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Set log level for this module, must come before esp_log.h
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
// ESP-IDF system services
#include "esp_log.h"
#include "esp_timer.h"

// Project specific headers
#include "button.h"
#include "project_config.h"


static const char *TAG = "Button";

/**
 * @brief Button state machine states
 */
typedef enum {
    BUTTON_WAIT_FOR_PRESS,          ///< Waiting for initial press
    BUTTON_DEBOUNCE_PRESS,          ///< Debouncing press event
    BUTTON_WAIT_FOR_RELEASE,        ///< Monitoring button release
    BUTTON_DEBOUNCE_RELEASE,        ///< Debouncing release event
    BUTTON_WAIT_FOR_DOUBLE,         ///< Checking for double click
    BUTTON_TIMEOUT_WAIT_FOR_RELEASE ///< Timeout waiting for release
} button_state_t;

/**
 * @brief Complete button instance structure
 */
struct button_s {
    gpio_num_t pin;                 ///< GPIO pin number
    button_state_t state;           ///< Current state machine state
    uint32_t last_time_ms;          ///< Last event timestamp
    uint32_t press_start_time_ms;   ///< Press start timestamp
    bool first_click;               ///< First click in double-click sequence

    bool active_low;                ///< Active low configuration
    uint32_t debounce_press_ms;     ///< Press debounce duration
    uint32_t debounce_release_ms;   ///< Release debounce duration
    uint32_t double_click_ms;       ///< Double click interval
    uint32_t long_click_ms;         ///< Long click duration
    uint32_t very_long_click_ms;    ///< Very long click duration
    uint32_t timeout_ms;            ///< Press timeout duration

    QueueHandle_t output_queue;     ///< Event output queue
    TaskHandle_t task_handle;       ///< Associated task handle
};

/**
 * @brief Get current time in milliseconds
 * @return Current timestamp in ms
 */
static uint32_t get_current_time_ms() { 
    return esp_timer_get_time() / 1000; 
}

/**
 * @brief Main button state machine logic
 * 
 * @param btn Button instance
 * @return Detected click type
 * 
 * @note This function implements a 6-state finite state machine:
 * 1. BUTTON_WAIT_FOR_PRESS - Waiting for initial press
 * 2. BUTTON_DEBOUNCE_PRESS - Debouncing press event
 * 3. BUTTON_WAIT_FOR_RELEASE - Monitoring button release
 * 4. BUTTON_DEBOUNCE_RELEASE - Debouncing release event
 * 5. BUTTON_WAIT_FOR_DOUBLE - Checking for double click
 * 6. BUTTON_TIMEOUT_WAIT_FOR_RELEASE - Timeout waiting for release
 */
static button_click_type_t button_get_click(button_t *btn) {
    uint32_t now = get_current_time_ms();
    uint8_t pressed_level = btn->active_low ? 0 : 1;
    uint8_t released_level = btn->active_low ? 1 : 0;

    switch (btn->state) {
    // Step 1: Wait for the button to be pressed
    case BUTTON_WAIT_FOR_PRESS:
        if (gpio_get_level(btn->pin) == pressed_level) {
            // Button pressed, start debounce timer and move to next state
            btn->press_start_time_ms = now;
            btn->state = BUTTON_DEBOUNCE_PRESS;
        }
        break;

    // Step 2: Debounce the button press to filter out noise
    case BUTTON_DEBOUNCE_PRESS:
        if (now - btn->press_start_time_ms > btn->debounce_press_ms) {
            // Debounce time has passed. Check if button is still pressed
            if (gpio_get_level(btn->pin) == pressed_level) {
                // Still pressed: valid press, wait for release
                btn->state = BUTTON_WAIT_FOR_RELEASE;
            } else {
                // Not pressed anymore: it was a glitch, go back to waiting
                btn->state = BUTTON_WAIT_FOR_PRESS;
            }
        }
        break;

    // Step 3: Wait for the button to be released
    case BUTTON_WAIT_FOR_RELEASE:
        if (gpio_get_level(btn->pin) == released_level) {
            // Button released. Check how long it was pressed
            uint32_t duration = now - btn->press_start_time_ms;

            if (duration > btn->very_long_click_ms) {
                // Held for a very long time
                btn->state = BUTTON_WAIT_FOR_PRESS;
                return BUTTON_VERY_LONG_CLICK;
            } else if (duration > btn->long_click_ms) {
                // Held for a long time
                btn->state = BUTTON_WAIT_FOR_PRESS;
                return BUTTON_LONG_CLICK;
            } else {
                // Short press. Start release debounce and check for double click
                btn->last_time_ms = now;
                btn->state = BUTTON_DEBOUNCE_RELEASE;
            }
        } else if (now - btn->press_start_time_ms > btn->timeout_ms) {
            // Button held down for too long (timeout)
            btn->state = BUTTON_TIMEOUT_WAIT_FOR_RELEASE;
            btn->last_time_ms = now;
        }
        break;

    // Step 4: Debounce the button release
    case BUTTON_DEBOUNCE_RELEASE:
        if (now - btn->last_time_ms > btn->debounce_release_ms) {
            // Release confirmed. Now check for a potential double click
            if (gpio_get_level(btn->pin) == released_level) {
                btn->state = BUTTON_WAIT_FOR_DOUBLE;
            } else {
                // If pressed again during debounce, proceed to double click check
                btn->state = BUTTON_WAIT_FOR_DOUBLE;
            }
        }
        break;

    // Step 5: Wait for a potential second click
    case BUTTON_WAIT_FOR_DOUBLE:
        // Check if a second press occurs within the double click window
        if (gpio_get_level(btn->pin) == pressed_level && !btn->first_click) {
            // Second press detected, confirming a double click sequence
            btn->last_time_ms = now;
            btn->first_click = true; // Flag that we are in a double click
            btn->state = BUTTON_DEBOUNCE_PRESS; // Restart debounce for the second click
        } else if (now - btn->last_time_ms > btn->double_click_ms) {
            // No second click occurred in time
            btn->state = BUTTON_WAIT_FOR_PRESS;
            if (btn->first_click) {
                // If we were in a double click sequence, it's now complete
                btn->first_click = false;
                return BUTTON_DOUBLE_CLICK;
            } else {
                // Otherwise, it was just a single click
                return BUTTON_CLICK;
            }
        }
        break;

    // Step 6: Handle button release after a timeout
    case BUTTON_TIMEOUT_WAIT_FOR_RELEASE:
        if (gpio_get_level(btn->pin) == released_level) {
            // Button finally released after a timeout
            if (now - btn->last_time_ms > btn->debounce_release_ms) {
                btn->last_time_ms = now;
                btn->state = BUTTON_WAIT_FOR_PRESS;
                return BUTTON_TIMEOUT;
            }
        } else {
            // If still held, check for an error condition (held way too long)
            btn->last_time_ms = now;
            // Double timeout period for error detection (2x normal timeout)
            if (now - btn->press_start_time_ms > 2 * btn->timeout_ms) {
                btn->state = BUTTON_WAIT_for_press;
                return BUTTON_ERROR;
            }
        }
        break;
    }

    return BUTTON_NONE_CLICK;
}

/**
 * @brief Button task main function
 * 
 * @param param Button instance passed as parameter
 * 
 * @note This task processes button events when notified by the ISR.
 * It disables interrupts during processing to prevent re-entrancy.
 */
static void button_task(void *param) {
    button_t *btn = (button_t *)param;
    bool processing = false;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!processing) {
            processing = true;
            gpio_intr_disable(btn->pin);
        }

        do {
            button_click_type_t click = button_get_click(btn);

            if (click != BUTTON_NONE_CLICK) {
                button_event_t local_event = {
                    .type = click,
                    .pin = btn->pin
                };

                if (xQueueSend(btn->output_queue, &local_event,
                              pdMS_TO_TICKS(10)) != pdPASS) {
                    ESP_LOGW(TAG, "Button %d: click %d failed to send to queue",
                            btn->pin, click);
                }

                processing = false;
                gpio_intr_enable(btn->pin);
                break;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        } while (processing);
    }
}

/**
 * @brief Button interrupt service routine
 * 
 * @param arg Button instance passed as argument
 * 
 * @note This ISR runs in IRAM for fast execution and notifies the button task.
 * It uses DRAM logging to avoid flash access during interrupt handling.
 */
static void IRAM_ATTR button_isr_handler(void *arg) {
    button_t *btn = (button_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    xTaskNotifyFromISR(btn->task_handle, 0, eNoAction,
                      &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Create a new button instance
 * 
 * @param config Button configuration
 * @param output_queue Event output queue
 * @return button_t* Button handle, NULL on error
 * 
 * @note The output queue must be created beforehand with adequate size
 * @warning Do not call this function from an ISR
 */
button_t *button_create(const button_config_t *config,
                       QueueHandle_t output_queue) {
    if (!config || !output_queue) {
        ESP_LOGE(TAG, "Invalid arguments: config or output_queue is NULL");
        return NULL;
    }

    button_t *btn = calloc(1, sizeof(button_t));
    if (!btn) {
        ESP_LOGE(TAG, "Failed to allocate memory for button struct");
        return NULL;
    }

    // Initialize configuration
    btn->pin = config->pin;
    btn->active_low = config->active_low;
    btn->output_queue = output_queue;
    btn->state = BUTTON_WAIT_FOR_PRESS;
    btn->first_click = false;

    // Set timing parameters
    btn->debounce_press_ms = config->debounce_press_ms;
    btn->debounce_release_ms = config->debounce_release_ms;
    btn->double_click_ms = config->double_click_ms;
    btn->long_click_ms = config->long_click_ms;
    btn->very_long_click_ms = config->very_long_click_ms;
    btn->timeout_ms = config->very_long_click_ms * 2; // Timeout is 2x very_long time

    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = config->active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = config->active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = config->active_low ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE
    };

    esp_err_t gpio_err = gpio_config(&io_conf);
    if (gpio_err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed for pin %d: %s", 
                config->pin, esp_err_to_name(gpio_err));
        free(btn);
        return NULL;
    }

    // Install ISR service (ignores error if already installed)
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s",
                esp_err_to_name(err));
        free(btn);
        return NULL;
    }

    gpio_isr_handler_add(config->pin, button_isr_handler, btn);

    // Create processing task
    BaseType_t res = xTaskCreate(button_task, "button_task", 
                               BUTTON_TASK_STACK_SIZE, btn, BUTTON_TASK_PRIORITY,
                               &btn->task_handle);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task for pin %d", config->pin);
        gpio_isr_handler_remove(config->pin);
        free(btn);
        return NULL;
    }

    ESP_LOGI(TAG, "Button created on pin %d (Active: %s)", 
            config->pin, config->active_low ? "LOW" : "HIGH");
    return btn;
}

/**
 * @brief Delete button instance and free resources
 * 
 * @param btn Button handle to delete
 * 
 * @note This function does not delete the event queue
 * @warning Ensure no tasks are using the button before deletion
 */
void button_delete(button_t *btn) {
    if (btn) {
        if (btn->task_handle) {
            vTaskDelete(btn->task_handle);
            ESP_LOGI(TAG, "Button task on pin %d deleted", btn->pin);
        }
        gpio_isr_handler_remove(btn->pin);
        ESP_LOGI(TAG, "Button on pin %d deleted", btn->pin);
        free(btn);
    }
}
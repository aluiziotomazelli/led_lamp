/**
 * @file switch.c
 * @brief Switch driver implementation with debouncing
 * 
 * @details This file implements a switch driver with hardware debouncing,
 * interrupt handling, and event queuing for reliable switch state detection.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <stdlib.h>

// ESP-IDF drivers
#include "driver/gpio.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Set log level for this module, must come before esp_log.h
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

// Project specific headers
#include "switch.h"
#include "project_config.h"

static const char *TAG = "Switch";

/**
 * @brief Complete switch instance structure
 * 
 * Holds all state for a single switch instance including
 * GPIO configuration, debouncing timing, and task handles
 */
struct switch_s {
    gpio_num_t pin;             ///< GPIO pin number
    bool active_low;            ///< Active low configuration
    uint16_t debounce_ms;       ///< Debounce time in milliseconds
    QueueHandle_t output_queue; ///< Event output queue
    TaskHandle_t task_handle;   ///< Associated task handle for notifications
};

/**
 * @brief ISR handler for the switch GPIO pin
 * 
 * @param arg Pointer to the switch_s instance
 * 
 * @note This ISR runs in IRAM for fast execution
 * @warning Keep ISR processing time minimal
 */
static void IRAM_ATTR switch_isr_handler(void *arg) {
    switch_t sw = (switch_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Give notification to the handler task
    vTaskNotifyGiveFromISR(sw->task_handle, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Task to handle switch state changes with debouncing
 * 
 * @param arg Pointer to the switch_s instance
 * 
 * @note This task handles debouncing by disabling interrupts during
 *       the debounce period and reading the stable pin state
 */
static void switch_task(void *arg) {
    switch_t sw = (switch_t)arg;

    // Allow system stabilization before first read
    vTaskDelay(pdMS_TO_TICKS(100));

    // Perform initial read to send boot-up state
    ESP_LOGI(TAG, "Performing initial state read for switch on pin %d", sw->pin);
    xTaskNotifyGive(sw->task_handle); // Trigger first read immediately

    while (1) {
        // Wait for notification from ISR or initial trigger
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0) {
            // Disable interrupts during debounce period
            gpio_intr_disable(sw->pin);

            // Wait for debounce period to settle
            vTaskDelay(pdMS_TO_TICKS(sw->debounce_ms));

            // Read stable state of the pin
            int level = gpio_get_level(sw->pin);

            // Create and send the event
            switch_event_t event = {
                .pin = sw->pin,
                .is_closed = (level == (sw->active_low ? 0 : 1))
            };

            if (xQueueSend(sw->output_queue, &event, pdMS_TO_TICKS(10)) != pdPASS) {
                ESP_LOGW(TAG, "Failed to send switch event to queue for pin %d", sw->pin);
            } else {
                ESP_LOGI(TAG, "Switch on pin %d state changed to: %s", 
                        sw->pin, event.is_closed ? "CLOSED" : "OPEN");
            }

            // Re-enable interrupts for next event
            gpio_intr_enable(sw->pin);
        }
    }
}

/**
 * @brief Create a new switch instance
 * 
 * @param[in] config Pointer to the switch configuration structure
 * @param[in] queue The queue to which switch events will be sent
 * @return switch_t Handle to the created switch instance, or NULL on failure
 * 
 * @note The output queue must be created before calling this function
 * @warning GPIO pins must have appropriate pull-up/down resistors
 */
switch_t switch_create(const switch_config_t *config, QueueHandle_t queue) {
    if (config == NULL || queue == NULL) {
        ESP_LOGE(TAG, "Invalid arguments for switch_create");
        return NULL;
    }

    // Allocate memory for the switch instance
    switch_t sw = calloc(1, sizeof(struct switch_s));
    if (sw == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for switch instance");
        return NULL;
    }

    // Copy configuration parameters
    sw->pin = config->pin;
    sw->active_low = config->active_low;
    sw->debounce_ms = config->debounce_ms;
    sw->output_queue = queue;

    ESP_LOGI(TAG, "Creating switch on GPIO %d", sw->pin);

    // Configure GPIO pin with interrupt on any edge
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << sw->pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = sw->active_low ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = sw->active_low ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed for pin %d: %s", sw->pin, esp_err_to_name(err));
        free(sw);
        return NULL;
    }

    // Create switch handling task with unique name
    char task_name[configMAX_TASK_NAME_LEN];
    snprintf(task_name, sizeof(task_name), "switch_task_%d", sw->pin);
    
    BaseType_t task_created = xTaskCreate(switch_task, task_name, 
                                        SWITCH_TASK_STACK_SIZE, sw, 
                                        SWITCH_TASK_PRIORITY, &sw->task_handle);
    
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create switch task for pin %d", sw->pin);
        free(sw);
        return NULL;
    }

    // Install ISR service (safe to call multiple times)
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
        vTaskDelete(sw->task_handle);
        free(sw);
        return NULL;
    }

    // Attach ISR handler to the GPIO pin
    err = gpio_isr_handler_add(sw->pin, switch_isr_handler, sw);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for pin %d: %s", sw->pin, esp_err_to_name(err));
        vTaskDelete(sw->task_handle);
        free(sw);
        return NULL;
    }

    ESP_LOGI(TAG, "Switch component created successfully for pin %d", sw->pin);
    return sw;
}

/**
 * @brief Delete a switch instance and free its resources
 * 
 * @param[in] sw The switch handle to delete
 * 
 * @note This function does not delete the event queue
 * @warning Ensure no tasks are using the switch before deletion
 */
void switch_delete(switch_t sw) {
    if (sw == NULL) {
        return;
    }
    
    ESP_LOGI(TAG, "Deleting switch on pin %d", sw->pin);
    
    // Remove ISR handler
    gpio_isr_handler_remove(sw->pin);
    
    // Delete processing task
    if (sw->task_handle) {
        vTaskDelete(sw->task_handle);
    }
    
    // Free memory
    free(sw);
}
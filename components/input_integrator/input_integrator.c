/**
 * @file input_integrator.c
 * @brief Input event integration implementation
 * 
 * @details This file implements a queue-based input integrator that combines
 * events from multiple input sources (buttons, encoders, touch, switches, ESP-NOW)
 * into a single unified event stream using FreeRTOS queue sets.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <stdlib.h>

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Set log level for this module, must come before esp_log.h
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
// ESP-IDF system services
#include "esp_log.h"
#include "esp_mac.h"

// Input device drivers
#include "button.h"
#include "encoder.h"
#include "touch.h"
#include "switch.h"

// Project specific headers
#include "input_integrator.h"
#include "project_config.h"

static const char *TAG = "InputIntegrator";

// Static instance of the queue manager, encapsulated within this component
static queue_manager_t s_queue_manager;

/**
 * @brief Main input integration task function
 * 
 * @param[in] pvParameters Unused
 * 
 * @note This task uses FreeRTOS queue sets to monitor multiple queues
 *       simultaneously and integrates events into a single output stream
 * @warning Task should have sufficient stack size for queue operations
 */
void integrator_task(void *pvParameters) {
    // This task now uses the static s_queue_manager instance
    integrated_event_t event;

    configASSERT(s_queue_manager.queue_set != NULL);

    ESP_LOGI(TAG, "Input integrator task started");

    while (1) {
        // Wait for activity on any queue in the set
        QueueHandle_t active_queue = xQueueSelectFromSet(s_queue_manager.queue_set, portMAX_DELAY);

        // Process button events
        if (active_queue == s_queue_manager.button_queue) {
            button_event_t button_evt;
            if (xQueueReceive(s_queue_manager.button_queue, &button_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_BUTTON;
                event.timestamp = xTaskGetTickCount();
                event.data.button = button_evt;
                xQueueSend(s_queue_manager.integrated_queue, &event, portMAX_DELAY);
                ESP_LOGD(TAG, "Integrated button event from pin %d", button_evt.pin);
            }
        } 
        // Process encoder events
        else if (active_queue == s_queue_manager.encoder_queue) {
            encoder_event_t encoder_evt;
            if (xQueueReceive(s_queue_manager.encoder_queue, &encoder_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_ENCODER;
                event.timestamp = xTaskGetTickCount();
                event.data.encoder = encoder_evt;
                xQueueSend(s_queue_manager.integrated_queue, &event, portMAX_DELAY);
                ESP_LOGD(TAG, "Integrated encoder event: %"PRIu32" steps", encoder_evt.steps);
            }
        } 
        // Process ESPNOW events
        else if (active_queue == s_queue_manager.espnow_queue) {
            espnow_event_t espnow_evt;
            if (xQueueReceive(s_queue_manager.espnow_queue, &espnow_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_ESPNOW;
                event.timestamp = xTaskGetTickCount();
                event.data.espnow = espnow_evt;
                xQueueSend(s_queue_manager.integrated_queue, &event, portMAX_DELAY);
                ESP_LOGD(TAG, "Integrated ESP-NOW event from " MACSTR, 
                        MAC2STR(espnow_evt.mac_addr));
            }
        } 
        // Process touch events
        else if (active_queue == s_queue_manager.touch_queue) {
            touch_event_t touch_evt;
            if (xQueueReceive(s_queue_manager.touch_queue, &touch_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_TOUCH;
                event.timestamp = xTaskGetTickCount();
                event.data.touch = touch_evt;
                xQueueSend(s_queue_manager.integrated_queue, &event, portMAX_DELAY);
                ESP_LOGD(TAG, "Integrated touch event from pad %d", touch_evt.pad);
            }
        }
        // Process switch events
        else if (active_queue == s_queue_manager.switch_queue) {
            switch_event_t switch_evt;
            if (xQueueReceive(s_queue_manager.switch_queue, &switch_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_SWITCH;
                event.timestamp = xTaskGetTickCount();
                event.data.switch_evt = switch_evt;
                xQueueSend(s_queue_manager.integrated_queue, &event, portMAX_DELAY);
                ESP_LOGD(TAG, "Integrated switch event from pin %d", switch_evt.pin);
            }
        }
    }
}

/**
 * @brief Initialize the queue manager structure
 * 
 * @param[in] btn_q Button events queue
 * @param[in] enc_q Encoder events queue
 * @param[in] espnow_q ESPNOW events queue
 * @param[in] touch_q Touch events queue
 * @param[in] switch_q Switch events queue
 * @param[in] int_q Integrated output queue
 * @return esp_err_t `ESP_OK` on success, or an error code on failure
 * 
 * @note This function creates a queue set and adds all input queues to it
 * @warning All input queues must be valid and created before calling this function
 */
esp_err_t init_queue_manager(QueueHandle_t btn_q, QueueHandle_t enc_q,
                                 QueueHandle_t espnow_q, QueueHandle_t touch_q,
                                 QueueHandle_t switch_q, QueueHandle_t int_q) {

    // Initialize the queue manager structure
    s_queue_manager.button_queue = btn_q;
    s_queue_manager.encoder_queue = enc_q;
    s_queue_manager.espnow_queue = espnow_q;
    s_queue_manager.touch_queue = touch_q;
    s_queue_manager.switch_queue = switch_q;
    s_queue_manager.integrated_queue = int_q;
    s_queue_manager.queue_set = NULL;

    // Validate input queues
    if (!btn_q || !enc_q || !espnow_q || !touch_q || !switch_q || !int_q) {
        ESP_LOGE(TAG, "One or more invalid queues received");
        return ESP_ERR_INVALID_ARG;
    }

    // Calculate total queue size for the queue set
    UBaseType_t queue_size = BUTTON_QUEUE_SIZE + ENCODER_QUEUE_SIZE + 
                            ESPNOW_QUEUE_SIZE + TOUCH_QUEUE_SIZE + SWITCH_QUEUE_SIZE;

    // Create queue set with sufficient capacity
    s_queue_manager.queue_set = xQueueCreateSet(queue_size);
    if (s_queue_manager.queue_set == NULL) {
        ESP_LOGE(TAG, "Failed to create queue set");
        return ESP_FAIL;
    }

    // Add all input queues to the queue set
    if (xQueueAddToSet(btn_q, s_queue_manager.queue_set) != pdPASS ||
        xQueueAddToSet(enc_q, s_queue_manager.queue_set) != pdPASS ||
        xQueueAddToSet(espnow_q, s_queue_manager.queue_set) != pdPASS ||
        xQueueAddToSet(touch_q, s_queue_manager.queue_set) != pdPASS ||
        xQueueAddToSet(switch_q, s_queue_manager.queue_set) != pdPASS) {
        ESP_LOGE(TAG, "Failed to add queues to set");
        vQueueDelete(s_queue_manager.queue_set);
        s_queue_manager.queue_set = NULL;
        return ESP_FAIL;
    }

    // Create the integrator task
	BaseType_t task_created = xTaskCreate(integrator_task, "integrator_task",
							   INTEGRATOR_TASK_STACK_SIZE, NULL, // Pass NULL as param
							   INTEGRATOR_TASK_PRIORITY, NULL);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create integrator task");
        vQueueDelete(s_queue_manager.queue_set);
        s_queue_manager.queue_set = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Queue manager initialized successfully with %d queue slots", queue_size);
    return ESP_OK;
}
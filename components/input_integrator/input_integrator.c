#include "input_integrator.h"
#include "button.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdlib.h>
#include "project_config.h"

static const char *TAG = "InputIntegrator";

/**
 * @brief Main input integration task function
 * @param pvParameters Pointer to queue manager structure
 */
void integrator_task(void *pvParameters) {
    configASSERT(pvParameters != NULL);
    queue_manager_t *qm = (queue_manager_t *)pvParameters;
    integrated_event_t event;

    // Validate parameters
    if (qm == NULL || qm->queue_set == NULL) {
        ESP_LOGE(TAG, "Invalid queue manager parameters");
        vTaskDelete(NULL);
    }
    
    configASSERT(qm->queue_set != NULL);

    while (1) {
        // Wait for activity on any queue
        QueueHandle_t active_queue = xQueueSelectFromSet(qm->queue_set, portMAX_DELAY);

        // Process button events
        if (active_queue == qm->button_queue) {
            button_event_t button_evt;
            if (xQueueReceive(qm->button_queue, &button_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_BUTTON;
                event.timestamp = xTaskGetTickCount();
                event.data.button = button_evt;
                xQueueSend(qm->integrated_queue, &event, portMAX_DELAY);
            }
        } 
        // Process encoder events
        else if (active_queue == qm->encoder_queue) {
            encoder_event_t encoder_evt;
            if (xQueueReceive(qm->encoder_queue, &encoder_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_ENCODER;
                event.timestamp = xTaskGetTickCount();
                event.data.encoder = encoder_evt;
                xQueueSend(qm->integrated_queue, &event, portMAX_DELAY);
            }
        } 
        // Process ESPNOW events
        else if (active_queue == qm->espnow_queue) {
            espnow_event_t espnow_evt;
            if (xQueueReceive(qm->espnow_queue, &espnow_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_ESPNOW;
                event.timestamp = xTaskGetTickCount();
                event.data.espnow = espnow_evt;
                xQueueSend(qm->integrated_queue, &event, portMAX_DELAY);
            }
        } 
        // Process touch events
        else if (active_queue == qm->touch_queue) {
            touch_event_t touch_evt;
            if (xQueueReceive(qm->touch_queue, &touch_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_TOUCH;
                event.timestamp = xTaskGetTickCount();
                event.data.touch = touch_evt;
                xQueueSend(qm->integrated_queue, &event, portMAX_DELAY);
            }
        }
        // Process switch events
        else if (active_queue == qm->switch_queue) {
            switch_event_t switch_evt;
            if (xQueueReceive(qm->switch_queue, &switch_evt, 0) == pdTRUE) {
                event.source = EVENT_SOURCE_SWITCH;
                event.timestamp = xTaskGetTickCount();
                event.data.switch_evt = switch_evt;
                xQueueSend(qm->integrated_queue, &event, portMAX_DELAY);
            }
        }
    }
}

/**
 * @brief Initialize the queue manager structure
 * @param btn_q Button events queue
 * @param enc_q Encoder events queue
 * @param espnow_q ESPNOW events queue
 * @param touch_q Touch events queue
 * @param switch_q Switch events queue
 * @param int_q Integrated output queue
 * @return Initialized queue manager structure
 */
queue_manager_t init_queue_manager(QueueHandle_t btn_q, QueueHandle_t enc_q,
                                 QueueHandle_t espnow_q, QueueHandle_t touch_q,
                                 QueueHandle_t switch_q, QueueHandle_t int_q) {
    queue_manager_t qm = {
        .button_queue = btn_q,
        .encoder_queue = enc_q,
        .espnow_queue = espnow_q,
        .touch_queue = touch_q,
        .switch_queue = switch_q,
        .integrated_queue = int_q,
        .queue_set = NULL
    };

    // Validate input queues
    if (!btn_q || !enc_q || !espnow_q || !touch_q || !switch_q || !int_q) {
        ESP_LOGE(TAG, "One or more invalid queues received");
        return qm;
    }

    // Calculate total queue size for the queue set
    UBaseType_t queue_size = BUTTON_QUEUE_SIZE + ENCODER_QUEUE_SIZE + 
                            ESPNOW_QUEUE_SIZE + TOUCH_QUEUE_SIZE + SWITCH_QUEUE_SIZE;

    // Create queue set
    qm.queue_set = xQueueCreateSet(queue_size);
    if (qm.queue_set == NULL) {
        ESP_LOGE(TAG, "Failed to create queue set");
        return qm;
    }

    // Add all queues to the set
    if (xQueueAddToSet(btn_q, qm.queue_set) != pdPASS ||
        xQueueAddToSet(enc_q, qm.queue_set) != pdPASS ||
        xQueueAddToSet(espnow_q, qm.queue_set) != pdPASS ||
        xQueueAddToSet(touch_q, qm.queue_set) != pdPASS ||
        xQueueAddToSet(switch_q, qm.queue_set) != pdPASS) {
        ESP_LOGE(TAG, "Failed to add queues to set");
        vQueueDelete(qm.queue_set);
        qm.queue_set = NULL;
    }

    ESP_LOGI(TAG, "Queue manager initialized successfully");
    return qm;
}
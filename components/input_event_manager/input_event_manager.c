#include <stdlib.h> // For malloc and free
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "input_event_manager.h" // For flags (BUTTON_FLAG, ENCODER_FLAG)
#include "button.h"    // For button_event_t
#include "encoder.h"   // For encoder_event_t

static const char *TAG_BTN_ADAPTER = "btn_adapter";
static const char *TAG_ENC_ADAPTER = "enc_adapter";

// Parameter Structures are defined in the .h file as they are needed by main.c

void button_adapter_task(void *pvParameters) {
    adapter_task_params_t *params = (adapter_task_params_t *)pvParameters;
    if (!params || !params->component_queue || !params->central_task_handle) {
        ESP_LOGE(TAG_BTN_ADAPTER, "Invalid parameters. Exiting.");
        if(params) free(params); // Free params if allocated
        vTaskDelete(NULL);
        return;
    }

    QueueHandle_t button_queue = params->component_queue;
    TaskHandle_t central_task = params->central_task_handle;
    button_event_t dummy_event; // Needed for xQueuePeek's pvBuffer argument

    ESP_LOGI(TAG_BTN_ADAPTER, "Button adapter task started. Waiting for events on queue %p, notifying task %p", button_queue, central_task);

    while (1) {
        // Wait indefinitely for an item to appear in the button queue
        if (xQueuePeek(button_queue, &dummy_event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGD(TAG_BTN_ADAPTER, "Button event detected on queue. Notifying central task.");
            // Notify the central consumer task
            xTaskNotify(central_task, BUTTON_FLAG, eSetBits);
            // Small delay to allow the central task to process.
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void encoder_adapter_task(void *pvParameters) {
    adapter_task_params_t *params = (adapter_task_params_t *)pvParameters;
    if (!params || !params->component_queue || !params->central_task_handle) {
        ESP_LOGE(TAG_ENC_ADAPTER, "Invalid parameters. Exiting.");
        if(params) free(params); // Free params if allocated
        vTaskDelete(NULL);
        return;
    }

    QueueHandle_t encoder_queue = params->component_queue;
    TaskHandle_t central_task = params->central_task_handle;
    encoder_event_t dummy_event; // Needed for xQueuePeek

    ESP_LOGI(TAG_ENC_ADAPTER, "Encoder adapter task started. Waiting for events on queue %p, notifying task %p", encoder_queue, central_task);

    while (1) {
        if (xQueuePeek(encoder_queue, &dummy_event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGD(TAG_ENC_ADAPTER, "Encoder event detected on queue. Notifying central task.");
            xTaskNotify(central_task, ENCODER_FLAG, eSetBits);
            // Similar concern as in button_adapter_task regarding re-notification.
            vTaskDelay(pdMS_TO_TICKS(50)); // Simple mitigation
        }
    }
}

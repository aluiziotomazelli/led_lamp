#include "switch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "project_config.h"
#include <stdlib.h>

static const char *TAG = "Switch";

/**
 * @brief Complete switch instance structure.
 * Holds all state for a single switch instance.
 */
struct switch_s {
    gpio_num_t pin;             ///< GPIO pin number.
    bool active_low;            ///< Active low configuration.
    uint16_t debounce_ms;       ///< Debounce time in ms.
    QueueHandle_t output_queue; ///< Event output queue.
    TaskHandle_t task_handle;   ///< Associated task handle for notifications.
};

/**
 * @brief ISR handler for the switch GPIO pin.
 *
 * @param arg Pointer to the switch_s instance.
 */
static void IRAM_ATTR switch_isr_handler(void *arg) {
    switch_t sw = (switch_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // Give a notification to the handler task. No need to check for yield,
    // as vTaskNotifyGiveFromISR handles it.
    vTaskNotifyGiveFromISR(sw->task_handle, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Task to handle switch state changes.
 *
 * @param arg Pointer to the switch_s instance.
 */
static void switch_task(void *arg) {
    switch_t sw = (switch_t)arg;

    // A small delay to allow the system to stabilize before the first read.
    vTaskDelay(pdMS_TO_TICKS(100));

    // Perform an initial read to send the boot-up state of the switch.
    ESP_LOGI(TAG, "Performing initial state read for switch on pin %d", sw->pin);
    xTaskNotifyGive(sw->task_handle); // Give notification to trigger the first read immediately.

    while (1) {
        // Wait indefinitely for a notification from the ISR (or the initial give above).
        // pdTRUE clears the notification value, making it behave like a binary semaphore.
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0) {
            // Disable interrupts during debounce
            gpio_intr_disable(sw->pin);

            // Wait for the debounce period
            vTaskDelay(pdMS_TO_TICKS(sw->debounce_ms));

            // Read the stable state of the pin
            int level = gpio_get_level(sw->pin);

            // Create and send the event
            switch_event_t event = {
                .pin = sw->pin,
                .is_closed = (level == (sw->active_low ? 0 : 1))
            };

            if (xQueueSend(sw->output_queue, &event, pdMS_TO_TICKS(10)) != pdPASS) {
                ESP_LOGW(TAG, "Failed to send switch event to queue for pin %d", sw->pin);
            } else {
                ESP_LOGI(TAG, "Switch on pin %d state changed to: %s", sw->pin, event.is_closed ? "CLOSED" : "OPEN");
            }

            // Re-enable interrupts for the next event
            gpio_intr_enable(sw->pin);
        }
    }
}

/**
 * @brief Create a new switch instance.
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

    // Copy configuration
    sw->pin = config->pin;
    sw->active_low = config->active_low;
    sw->debounce_ms = config->debounce_ms;
    sw->output_queue = queue;

    ESP_LOGI(TAG, "Creating switch on GPIO %d", sw->pin);

    // Configure GPIO pin
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

    // Create the switch handling task. The task handle is stored in our instance struct.
    char task_name[configMAX_TASK_NAME_LEN];
    snprintf(task_name, sizeof(task_name), "switch_task_%d", sw->pin);
    BaseType_t task_created = xTaskCreate(switch_task, task_name, SWITCH_TASK_STACK_SIZE, sw, SWITCH_TASK_PRIORITY, &sw->task_handle);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create switch task for pin %d", sw->pin);
        free(sw);
        return NULL;
    }

    // Install ISR service (it's safe to call this multiple times)
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
        vTaskDelete(sw->task_handle);
        free(sw);
        return NULL;
    }

    // Attach the ISR handler to the GPIO pin
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
 * @brief Delete a switch instance and free its resources.
 */
void switch_delete(switch_t sw) {
    if (sw == NULL) {
        return;
    }
    ESP_LOGI(TAG, "Deleting switch on pin %d", sw->pin);
    gpio_isr_handler_remove(sw->pin);
    if (sw->task_handle) {
        vTaskDelete(sw->task_handle);
    }
    free(sw);
}

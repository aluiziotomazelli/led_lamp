#include "switch.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "project_config.h"

#define SWITCH_TASK_STACK_SIZE 2048
#define SWITCH_TASK_PRIORITY   10
#define DEBOUNCE_DELAY_MS      50

static const char *TAG = "Switch";

// FreeRTOS objects
static QueueHandle_t g_switch_queue = NULL;
static SemaphoreHandle_t g_switch_sem = NULL;

// Store the pin number for the task
static gpio_num_t g_switch_pin;

/**
 * @brief ISR handler for the switch GPIO pin.
 *
 * This function is triggered on any edge (rising or falling) of the switch pin.
 * It gives a semaphore to unblock the switch_task for processing.
 *
 * @param arg The GPIO pin number, passed during ISR registration.
 */
static void IRAM_ATTR switch_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(g_switch_sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

/**
 * @brief Task to handle switch state changes.
 *
 * This task waits for a semaphore to be given by the ISR. Once unblocked,
 * it handles debouncing, reads the switch's state, and sends an event
 * to the queue.
 *
 * @param arg Unused.
 */
static void switch_task(void *arg) {
    while (1) {
        // Wait indefinitely for the semaphore from the ISR
        if (xSemaphoreTake(g_switch_sem, portMAX_DELAY) == pdTRUE) {
            // Disable interrupts during debounce
            gpio_intr_disable(g_switch_pin);

            // Wait for the debounce period
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY_MS));

            // Read the stable state of the pin
            int level = gpio_get_level(g_switch_pin);

            // Create and send the event
            switch_event_t event = {
                .pin = g_switch_pin,
                .is_closed = (level == 0) // Assuming active-low (closed = GND)
            };

            if (xQueueSend(g_switch_queue, &event, pdMS_TO_TICKS(10)) != pdPASS) {
                ESP_LOGW(TAG, "Failed to send switch event to queue");
            } else {
                ESP_LOGI(TAG, "Switch on pin %d state changed to: %s", g_switch_pin, event.is_closed ? "CLOSED" : "OPEN");
            }

            // Re-enable interrupts for the next event
            gpio_intr_enable(g_switch_pin);
        }
    }
}

/**
 * @brief Initialize the switch component.
 *
 * Configures the GPIO pin, installs the ISR, creates the handling task,
 * and sets up the necessary FreeRTOS objects (queue and semaphore).
 *
 * @param pin The GPIO pin number for the switch.
 * @param queue The queue to which switch events will be sent.
 * @return esp_err_t ESP_OK on success, or an error code on failure.
 */
esp_err_t switch_init(gpio_num_t pin, QueueHandle_t queue) {
    if (queue == NULL) {
        ESP_LOGE(TAG, "Queue handle is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    g_switch_queue = queue;
    g_switch_pin = pin;

    ESP_LOGI(TAG, "Initializing switch on GPIO %d", pin);

    // Create a binary semaphore to signal from ISR to task
    g_switch_sem = xSemaphoreCreateBinary();
    if (g_switch_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return ESP_ERR_NO_MEM;
    }

    // Configure GPIO pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, // Use internal pull-up for stability
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE     // Trigger on both rising and falling edges
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(err));
        return err;
    }

    // Create the switch handling task
    BaseType_t task_created = xTaskCreate(switch_task, "switch_task", SWITCH_TASK_STACK_SIZE, NULL, SWITCH_TASK_PRIORITY, NULL);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create switch task");
        return ESP_FAIL;
    }

    // Install ISR service if not already installed
    err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(err));
        return err;
    }

    // Attach the ISR handler to the GPIO pin
    err = gpio_isr_handler_add(pin, switch_isr_handler, (void *)pin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Switch component initialized successfully");
    return ESP_OK;
}

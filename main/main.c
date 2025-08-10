#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "project_config.h" // For queue sizes and pin definitions
#include "button.h"
#include "encoder.h"
#include "touch_button.h"
#include "input_integrator.h" // For integrated_event_t, init_queue_manager, integrator_task
#include <inttypes.h> // For PRIu32

// Define Global Variables
static const char *TAG = "main_test";

QueueHandle_t button_event_queue;
QueueHandle_t encoder_event_queue;
QueueHandle_t touch_button_event_queue;
QueueHandle_t espnow_event_queue; // Though not actively used for sending in this test, it's part of integrator
QueueHandle_t integrated_event_queue;

queue_manager_t queue_manager;

// Define stack sizes for tasks - these were previously hardcoded and might be a source of issues if too small
#define TASK_STACK_SIZE_INTEGRATOR 2048
#define TASK_STACK_SIZE_HANDLER 2048

// Implement integrated_event_handler_task
static void integrated_event_handler_task(void *pvParameters) {
    integrated_event_t event;
    while (1) {
        if (xQueueReceive(integrated_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.source) {
                case EVENT_SOURCE_BUTTON:
                    ESP_LOGI(TAG, "Integrated Event: BUTTON - Pin: %d, Type: %d, Timestamp: %" PRIu32,
                             event.data.button.pin, event.data.button.type, event.timestamp);
                    break;
                case EVENT_SOURCE_ENCODER:
                    ESP_LOGI(TAG, "Integrated Event: ENCODER - Steps: %" PRId32 ", Timestamp: %" PRIu32,
                             event.data.encoder.steps, event.timestamp);
                    break;
                case EVENT_SOURCE_ESPNOW:
                    // Ensure ESPNOW data is handled safely, e.g. check data_len before printing
                    ESP_LOGI(TAG, "Integrated Event: ESPNOW - MAC: %02x:%02x:%02x:%02x:%02x:%02x, DataLen: %d, Timestamp: %" PRIu32,
                             event.data.espnow.mac_addr[0], event.data.espnow.mac_addr[1],
                             event.data.espnow.mac_addr[2], event.data.espnow.mac_addr[3],
                             event.data.espnow.mac_addr[4], event.data.espnow.mac_addr[5],
                             event.data.espnow.data_len, event.timestamp);
                    // Example: if (event.data.espnow.data_len > 0) { ESP_LOGI(TAG, "  Data: %.*s", event.data.espnow.data_len, (char*)event.data.espnow.data); }
                    break;
                case EVENT_SOURCE_TOUCH:
                    ESP_LOGI(TAG, "Integrated Event: TOUCH - Pad: %d, Type: %d, Timestamp: %" PRIu32,
                             event.data.touch.touch_pad, event.data.touch.type, event.timestamp);
                    break;
                default:
                    ESP_LOGW(TAG, "Integrated Event: UNKNOWN SOURCE (%d), Timestamp: %" PRIu32, event.source, event.timestamp);
                    break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay
    }
}

void app_main(void) {
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Create Queues
    button_event_queue = xQueueCreate(BUTTON_QUEUE_SIZE, sizeof(button_event_t));
    configASSERT(button_event_queue != NULL);
    ESP_LOGI(TAG, "Button event queue created (size: %d)", BUTTON_QUEUE_SIZE);

    encoder_event_queue = xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoder_event_t));
    configASSERT(encoder_event_queue != NULL);
    ESP_LOGI(TAG, "Encoder event queue created (size: %d)", ENCODER_QUEUE_SIZE);

    touch_button_event_queue = xQueueCreate(TOUCH_BUTTON_QUEUE_SIZE, sizeof(touch_button_event_t));
    configASSERT(touch_button_event_queue != NULL);
    ESP_LOGI(TAG, "Touch button event queue created (size: %d)", TOUCH_BUTTON_QUEUE_SIZE);

    espnow_event_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    configASSERT(espnow_event_queue != NULL);
    ESP_LOGI(TAG, "ESP-NOW event queue created (size: %d)", ESPNOW_QUEUE_SIZE);

    UBaseType_t integrated_queue_len = BUTTON_QUEUE_SIZE + ENCODER_QUEUE_SIZE + ESPNOW_QUEUE_SIZE + TOUCH_BUTTON_QUEUE_SIZE;
    integrated_event_queue = xQueueCreate(integrated_queue_len, sizeof(integrated_event_t));
    configASSERT(integrated_event_queue != NULL);
    ESP_LOGI(TAG, "Integrated event queue created (size: %du)", integrated_queue_len);

    // Initialize Button
    button_config_t btn_cfg = {
        .pin = BUTTON1_PIN,
        .active_low = true,
        .debounce_press_ms = DEBOUNCE_PRESS_MS,
        .debounce_release_ms = DEBOUNCE_RELEASE_MS,
        .double_click_ms = DOUBLE_CLICK_MS,
        .long_click_ms = LONG_CLICK_MS,
        .very_long_click_ms = VERY_LONG_CLICK_MS
    };
    button_t *button_handle = button_create(&btn_cfg, button_event_queue);
    configASSERT(button_handle != NULL);
    ESP_LOGI(TAG, "Button initialized on pin %d", BUTTON1_PIN);

    // Initialize Touch Button
    touch_button_config_t touch_btn_cfg = {
        .touch_pad = TOUCH_BUTTON_PIN,
        .threshold_percent = 0.6f
    };
    touch_button_t *touch_button_handle = touch_button_create(&touch_btn_cfg, touch_button_event_queue);
    configASSERT(touch_button_handle != NULL);
    ESP_LOGI(TAG, "Touch button initialized on pad %d", TOUCH_BUTTON_PIN);

    // Initialize Encoder
    encoder_config_t enc_cfg = {
        .pin_a = ENCODER_PIN_A,
        .pin_b = ENCODER_PIN_B,
        .half_step_mode = false,
        .acceleration_enabled = true, // Keep it simple for testing
        .accel_gap_ms = ENC_ACCEL_GAP,
        .accel_max_multiplier = MAX_ACCEL_MULTIPLIER
    };
    encoder_handle_t encoder_handle = encoder_create(&enc_cfg, encoder_event_queue);
    configASSERT(encoder_handle != NULL);
    ESP_LOGI(TAG, "Encoder initialized on pins A: %d, B: %d", ENCODER_PIN_A, ENCODER_PIN_B);

    // Initialize Input Integrator
    queue_manager = init_queue_manager(button_event_queue, encoder_event_queue, espnow_event_queue, touch_button_event_queue, integrated_event_queue);
    configASSERT(queue_manager.queue_set != NULL);
    ESP_LOGI(TAG, "Input integrator initialized.");

    // Create Tasks
    BaseType_t task_created;
    task_created = xTaskCreate(integrator_task, "integrator_task", TASK_STACK_SIZE_INTEGRATOR, &queue_manager, 5, NULL);
    configASSERT(task_created == pdPASS);

    task_created = xTaskCreate(integrated_event_handler_task, "integrated_event_handler_task", TASK_STACK_SIZE_HANDLER, NULL, 4, NULL);
    configASSERT(task_created == pdPASS);
    
    ESP_LOGI(TAG, "Tasks created (integrator stack: %d, handler stack: %d).", TASK_STACK_SIZE_INTEGRATOR, TASK_STACK_SIZE_HANDLER);
    ESP_LOGI(TAG, "Test main setup complete. Monitoring events...");
}

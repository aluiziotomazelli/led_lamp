#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "project_config.h" // For queue sizes and pin definitions
#include "button.h"
#include "encoder.h"
#include "input_integrator.h" // For integrated_event_t, init_queue_manager, integrator_task
#include "fsm.h"
#include "led_controller.h"
#include <inttypes.h> // For PRIu32

// Define Global Variables
static const char *TAG = "main_app";

QueueHandle_t button_event_queue;
QueueHandle_t encoder_event_queue;
QueueHandle_t espnow_event_queue;
QueueHandle_t integrated_event_queue;

queue_manager_t queue_manager;

// Define stack sizes for tasks
#define TASK_STACK_SIZE_INTEGRATOR 2048

void app_main(void) {
    esp_log_level_set(TAG, ESP_LOG_INFO);

    // Create Queues
    button_event_queue = xQueueCreate(BUTTON_QUEUE_SIZE, sizeof(button_event_t));
    encoder_event_queue = xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoder_event_t));
    espnow_event_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    UBaseType_t integrated_queue_len = BUTTON_QUEUE_SIZE + ENCODER_QUEUE_SIZE + ESPNOW_QUEUE_SIZE;
    integrated_event_queue = xQueueCreate(integrated_queue_len, sizeof(integrated_event_t));

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

    // Initialize Encoder
    encoder_config_t enc_cfg = {
        .pin_a = ENCODER_PIN_A,
        .pin_b = ENCODER_PIN_B,
        .half_step_mode = false,
        .acceleration_enabled = true,
        .accel_gap_ms = ENC_ACCEL_GAP,
        .accel_max_multiplier = MAX_ACCEL_MULTIPLIER
    };
    encoder_handle_t encoder_handle = encoder_create(&enc_cfg, encoder_event_queue);
    configASSERT(encoder_handle != NULL);
    ESP_LOGI(TAG, "Encoder initialized on pins A:%d, B:%d", ENCODER_PIN_A, ENCODER_PIN_B);

    // Initialize Input Integrator
    queue_manager = init_queue_manager(button_event_queue, encoder_event_queue, espnow_event_queue, integrated_event_queue);
    configASSERT(queue_manager.queue_set != NULL);
    ESP_LOGI(TAG, "Input integrator initialized.");

    // Initialize LED Controller
    led_controller_config_t led_cfg = {
        .gpio_pin = LED_STRIP_PIN,
        .led_count = LED_STRIP_LED_COUNT,
        .task_stack_size = LED_RENDERER_STACK_SIZE,
        .task_priority = LED_RENDERER_PRIORITY
    };
    configASSERT(led_controller_init(&led_cfg) == ESP_OK);
    ESP_LOGI(TAG, "LED controller initialized.");

    // Initialize FSM
    // Passing NULL to use default config from project_config.h
    configASSERT(fsm_init(integrated_event_queue, NULL) == ESP_OK);
    ESP_LOGI(TAG, "FSM initialized.");

    // Create Input Integrator Task
    BaseType_t task_created = xTaskCreate(integrator_task, "integrator_task", TASK_STACK_SIZE_INTEGRATOR, &queue_manager, 5, NULL);
    configASSERT(task_created == pdPASS);
    
    // Trigger initial render
    led_controller_update_request();

    ESP_LOGI(TAG, "System setup complete. FSM and renderer are running.");
}

#include "esp_log_level.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "button.h"
#include "encoder.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "fsm.h"              // For FSM initialization
#include "input_integrator.h" // For integrated_event_t, init_queue_manager, integrator_task
#include "project_config.h" // For queue sizes and pin definitions
#include <inttypes.h>       // For PRIu32
#include <stdio.h>

// Define Global Variables
static const char *TAG = "main_test";

QueueHandle_t button_event_queue;
QueueHandle_t encoder_event_queue;
QueueHandle_t espnow_event_queue; // Though not actively used for sending in
                                  // this test, it's part of integrator
QueueHandle_t integrated_event_queue;
QueueHandle_t output_event_queue;

queue_manager_t queue_manager;

// Define stack sizes for tasks - these were previously hardcoded and might be a
// source of issues if too small



void app_main(void) {
  
   esp_log_level_set("*", ESP_LOG_DEBUG);
   esp_log_level_set("Button", ESP_LOG_INFO);
   esp_log_level_set("Button FSM", ESP_LOG_INFO);
   esp_log_level_set("INP_INTEGRATOR", ESP_LOG_INFO);

  // Create Queues
  button_event_queue = xQueueCreate(BUTTON_QUEUE_SIZE, sizeof(button_event_t));
  configASSERT(button_event_queue != NULL);
  ESP_LOGI(TAG, "Button event queue created (size: %d)", BUTTON_QUEUE_SIZE);

  encoder_event_queue =
      xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoder_event_t));
  configASSERT(encoder_event_queue != NULL);
  ESP_LOGI(TAG, "Encoder event queue created (size: %d)", ENCODER_QUEUE_SIZE);

  espnow_event_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
  configASSERT(espnow_event_queue != NULL);
  ESP_LOGI(TAG, "ESP-NOW event queue created (size: %d)", ESPNOW_QUEUE_SIZE);

  UBaseType_t integrated_queue_len =
      BUTTON_QUEUE_SIZE + ENCODER_QUEUE_SIZE + ESPNOW_QUEUE_SIZE;
  integrated_event_queue =
      xQueueCreate(integrated_queue_len, sizeof(integrated_event_t));
  configASSERT(integrated_event_queue != NULL);
  ESP_LOGI(TAG, "Integrated event queue created (size: %du)",
           integrated_queue_len);

  // Initialize Button
  button_config_t btn_cfg = {.pin = BUTTON1_PIN,
                             .active_low = true,
                             .debounce_press_ms = DEBOUNCE_PRESS_MS,
                             .debounce_release_ms = DEBOUNCE_RELEASE_MS,
                             .double_click_ms = DOUBLE_CLICK_MS,
                             .long_click_ms = LONG_CLICK_MS,
                             .very_long_click_ms = VERY_LONG_CLICK_MS};
  button_t *button_handle = button_create(&btn_cfg, button_event_queue);
  configASSERT(button_handle != NULL);
  ESP_LOGI(TAG, "Button initialized on pin %d", BUTTON1_PIN);

  // Initialize Encoder
  encoder_config_t enc_cfg = {.pin_a = ENCODER_PIN_A,
                              .pin_b = ENCODER_PIN_B,
                              .half_step_mode = true,
                              .acceleration_enabled =
                                  true, // Keep it simple for testing
                              .accel_gap_ms = ENC_ACCEL_GAP,
                              .accel_max_multiplier = MAX_ACCEL_MULTIPLIER};
  encoder_handle_t encoder_handle =
      encoder_create(&enc_cfg, encoder_event_queue);
  configASSERT(encoder_handle != NULL);
  ESP_LOGI(TAG, "Encoder initialized on pins A: %d, B: %d", ENCODER_PIN_A,
           ENCODER_PIN_B);

  // Initialize Input Integrator
  queue_manager =
      init_queue_manager(button_event_queue, encoder_event_queue,
                         espnow_event_queue, integrated_event_queue);
  configASSERT(queue_manager.queue_set != NULL);
  ESP_LOGI(TAG, "Input integrator initialized.");

  // Create Tasks
  BaseType_t task_created;
  task_created =
      xTaskCreate(integrator_task, "integrator_task",
                  TASK_STACK_SIZE_INTEGRATOR, &queue_manager, 5, NULL);
  configASSERT(task_created == pdPASS);
  ESP_LOGI(TAG, "Integrator task created.");

  output_event_queue =
      xQueueCreate(OUTPUT_QUEUE_SIZE, sizeof(fsm_output_action_t));
  configASSERT(output_event_queue != NULL);
  ESP_LOGI(TAG, "Output event queue created (size: %d)", OUTPUT_QUEUE_SIZE);

}
#include "button.h"
#include "encoder.h"
#include "esp_log_level.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "input_integrator.h" // For integrated_event_t, init_queue_manager, integrator_task
#include "project_config.h" // For queue sizes and pin definitions
#include "touch.h"
#include <inttypes.h> // For PRIu32
#include <stdio.h>

// Define Global Variables
static const char *TAG = "main";

QueueHandle_t button_event_queue;
QueueHandle_t encoder_event_queue;
QueueHandle_t espnow_event_queue; // Though not actively used for sending in
								  // this test, it's part of integrator
QueueHandle_t touch_event_queue;
QueueHandle_t integrated_event_queue;

queue_manager_t queue_manager;

// Define stack sizes for tasks - these were previously hardcoded and might be a
// source of issues if too small
#define TASK_STACK_SIZE_INTEGRATOR 2048
#define TASK_STACK_SIZE_HANDLER 2048

// Opcional: cria
// Implement integrated_event_handler_task
static void integrated_event_handler_task(void *pvParameters) {
	integrated_event_t event;
	
	// Inicializa o LED (GPIO2)
    const int LED_GPIO = 2;
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0); // Inicia desligado
    
	while (1) {
		if (xQueueReceive(integrated_event_queue, &event, portMAX_DELAY) ==
			pdTRUE) {
			switch (event.source) {
			case EVENT_SOURCE_BUTTON:
				ESP_LOGI(TAG,
						 "BUTTON - Pin: %d, Type: %d, "
						 "Timestamp: %" PRIu32,
						 event.data.button.pin, event.data.button.type,
						 event.timestamp);
				break;
			case EVENT_SOURCE_ENCODER:
				ESP_LOGI(TAG,
						 "ENCODER - Steps: %" PRId32
						 ", Timestamp: %" PRIu32,
						 event.data.encoder.steps, event.timestamp);
				break;
			case EVENT_SOURCE_ESPNOW:
				// Ensure ESPNOW data is handled safely, e.g. check data_len
				// before printing
				ESP_LOGI(TAG,
						 "ESPNOW - MAC: "
						 "%02x:%02x:%02x:%02x:%02x:%02x, DataLen: %d, "
						 "Timestamp: %" PRIu32,
						 event.data.espnow.mac_addr[0],
						 event.data.espnow.mac_addr[1],
						 event.data.espnow.mac_addr[2],
						 event.data.espnow.mac_addr[3],
						 event.data.espnow.mac_addr[4],
						 event.data.espnow.mac_addr[5],
						 event.data.espnow.data_len, event.timestamp);
				// Example: if (event.data.espnow.data_len > 0) { ESP_LOGI(TAG,
				// "  Data: %.*s", event.data.espnow.data_len,
				// (char*)event.data.espnow.data); }
				break;
			case EVENT_SOURCE_TOUCH:
				// Processamento específico para touch events
				switch (event.data.touch.type) {
				case TOUCH_PRESS:
				
				
					ESP_LOGI(TAG,  
							 "Touch press on pad %d (via integrator)",
							 event.data.touch.pad);
					// Sua lógica para toque simples aqui
					static bool led_state = false;
                    led_state = !led_state; // Alterna estado
                    gpio_set_level(LED_GPIO, led_state);
                    ESP_LOGI(TAG, "LED %s", led_state ? "LIGADO" : "DESLIGADO");
					
					break;

				case TOUCH_HOLD:
					ESP_LOGI(TAG,
							 "Touch hold press on pad %d (via integrator)",
							 event.data.touch.pad);
					// Sua lógica para toque longo aqui (ex: aumentar brilho)
					// --- NOVO: Piscar LED 3x para toque longo ---
                    led_state = !led_state; // Alterna estado
                    gpio_set_level(LED_GPIO, led_state);
                    ESP_LOGI(TAG, "LED %s", led_state ? "LIGADO" : "DESLIGADO");
					
					break;
//				case TOUCH_HOLD_REPEAT:
//					ESP_LOGI("Integrated Event: TOUCH",
//							 "Touch hold repeat on pad %d (via integrator)",
//							 event.data.touch.pad);
//					// Sua lógica para toque longo aqui (ex: aumentar brilho)
//					break;

				case TOUCH_ERROR:
					ESP_LOGW(TAG,
							 "Touch error on pad %d (via integrator)",
							 event.data.touch.pad);
					break;

				default:
					ESP_LOGW(TAG, "Unknown touch type %d on pad %d",
							 event.data.touch.type, event.data.touch.pad);
					break;
				}

				// Log padrão do integrator também
				ESP_LOGI(TAG,
						 "TOUCH - Pad: %d, Type: %d, "
						 "Timestamp: %" PRIu32,
						 event.data.touch.pad, event.data.touch.type,
						 event.timestamp);
				break;

			default:
				ESP_LOGW(TAG,
						 "UNKNOWN SOURCE (%d), Timestamp: "
						 "%" PRIu32,
						 event.source, event.timestamp);
				break;
			}
		}
		vTaskDelay(pdMS_TO_TICKS(10)); // Small delay
	}
}

void app_main(void) {
	esp_log_level_set("Touch", ESP_LOG_DEBUG);

	// Create Queues
	button_event_queue =
		xQueueCreate(BUTTON_QUEUE_SIZE, sizeof(button_event_t));
	configASSERT(button_event_queue != NULL);
	ESP_LOGI(TAG, "Button event queue created (size: %d)", BUTTON_QUEUE_SIZE);

	encoder_event_queue =
		xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoder_event_t));
	configASSERT(encoder_event_queue != NULL);
	ESP_LOGI(TAG, "Encoder event queue created (size: %d)", ENCODER_QUEUE_SIZE);

	touch_event_queue =
		xQueueCreate(TOUCH_QUEUE_SIZE, sizeof(touch_event_t));
	configASSERT(touch_event_queue != NULL);
	ESP_LOGI(TAG, "Touch button event queue created (size: %d)",
			 TOUCH_QUEUE_SIZE);

	espnow_event_queue =
		xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
	configASSERT(espnow_event_queue != NULL);
	ESP_LOGI(TAG, "ESP-NOW event queue created (size: %d)", ESPNOW_QUEUE_SIZE);

	UBaseType_t integrated_queue_len = BUTTON_QUEUE_SIZE + ENCODER_QUEUE_SIZE +
									   ESPNOW_QUEUE_SIZE +
									   TOUCH_QUEUE_SIZE;
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
								.half_step_mode = false,
								.acceleration_enabled =
									true, // Keep it simple for testing
								.accel_gap_ms = ENC_ACCEL_GAP,
								.accel_max_multiplier = MAX_ACCEL_MULTIPLIER};
	encoder_handle_t encoder_handle =
		encoder_create(&enc_cfg, encoder_event_queue);
	configASSERT(encoder_handle != NULL);
	ESP_LOGI(TAG, "Encoder initialized on pins A: %d, B: %d", ENCODER_PIN_A,
			 ENCODER_PIN_B);

// Initialize Touch
touch_config_t touch_cfg = {
        .pad = TOUCH_PAD1_PIN,
        .threshold_percent = TOUCH_THRESHOLD_PERCENT,
        .debounce_press_ms = TOUCH_DEBOUNCE_PRESS_MS,
        .debounce_release_ms = TOUCH_DEBOUNCE_RELEASE_MS,
        .hold_time_ms = TOUCH_HOLD_TIME_MS,
        .hold_repeat_interval_ms = TOUCH_HOLD_REPEAT_TIME_MS,
        .recalibration_interval_min = TOUCH_RECALIBRATION_INTERVAL_MIN,
        .enable_hold_repeat = true
    };

	touch_t *touch_handle =
		touch_create(&touch_cfg, touch_event_queue);
	configASSERT(touch_handle != NULL);
	ESP_LOGI(TAG, "Touch button initialized on pad %d", TOUCH_PAD1_PIN);

	// Initialize Input Integrator
	queue_manager = init_queue_manager(
		button_event_queue, encoder_event_queue, espnow_event_queue,
		touch_event_queue, integrated_event_queue);
	configASSERT(queue_manager.queue_set != NULL);
	ESP_LOGI(TAG, "Input integrator initialized.");

	// Create Tasks
	BaseType_t task_created;
	task_created =
		xTaskCreate(integrator_task, "integrator_task",
					TASK_STACK_SIZE_INTEGRATOR, &queue_manager, 5, NULL);
	configASSERT(task_created == pdPASS);

	task_created = xTaskCreate(integrated_event_handler_task,
							   "integrated_event_handler_task",
							   TASK_STACK_SIZE_HANDLER, NULL, 4, NULL);
	configASSERT(task_created == pdPASS);

	ESP_LOGI(TAG, "Tasks created (integrator stack: %d, handler stack: %d).",
			 TASK_STACK_SIZE_INTEGRATOR, TASK_STACK_SIZE_HANDLER);
	ESP_LOGI(TAG, "Test main setup complete. Monitoring events...");

}

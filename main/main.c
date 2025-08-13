#include "button.h"
#include "encoder.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "input_integrator.h"
#include "project_config.h"
#include "touch.h"
#include <inttypes.h>
#include <stdio.h>

#include "esp_system.h"
#include "fsm.h" // FSM incremental

static const char *TAG = "main";

// Filas globais
QueueHandle_t button_event_queue;
QueueHandle_t encoder_event_queue;
QueueHandle_t espnow_event_queue;
QueueHandle_t touch_event_queue;
QueueHandle_t integrated_event_queue;
QueueHandle_t led_cmd_queue; // Saída da FSM para o LED Manager

queue_manager_t queue_manager;

// -----------------------------------------------------------------------------
// Mock LED Manager Task
// -----------------------------------------------------------------------------
static void led_manager_task(void *pv) {
    led_command_t cmd;
    while (1) {
        if (xQueueReceive(led_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI("LED_MANAGER", "CMD %d | TS: %" PRIu64 " | Val: %d", 
                    cmd.cmd, cmd.timestamp, cmd.value);
            
            switch (cmd.cmd) {
                case LED_CMD_TURN_ON:
                    ESP_LOGI("LED_MANAGER", "Ligar LEDs");
                    break;

                case LED_CMD_TURN_ON_FADE:
                    ESP_LOGI("LED_MANAGER", "Ligar LED com Fade");
                    break;
                    
                case LED_CMD_TURN_OFF:
                    ESP_LOGI("LED_MANAGER", "Desligar LEDs");
                    break;
                    
                case LED_CMD_INC_BRIGHTNESS:
                    ESP_LOGI("LED_MANAGER", "Brilho += %d", cmd.value);
                    if(abs(cmd.value) > 10) {
                        ESP_LOGW("LED_MANAGER", "Grande ajuste de brilho!");
                    }
                    break;
                    
                case LED_CMD_INC_EFFECT:
                    ESP_LOGI("LED_MANAGER", "Efeito += %d", cmd.value);
                    break;
                    
                case LED_CMD_INC_EFFECT_PARAM:
                    ESP_LOGI("LED_MANAGER", "Parâmetro do efeito += %d", cmd.value);
                    break;
                
                case LED_CMD_NEXT_EFFECT_PARAM:
                    ESP_LOGI("LED_MANAGER", "Next Effect param");
                    break;
                    
                case LED_CMD_INC_SYSTEM_PARAM:
                    ESP_LOGI("LED_MANAGER", "Parâmetro do sistema += %d", cmd.value);
                    break;

                case LED_CMD_NEXT_SYSTEM_PARAM:
                    ESP_LOGI("LED_MANAGER", "Next System param");
                    break;
                    
                case LED_CMD_SET_EFFECT:
                    ESP_LOGI("LED_MANAGER", "Confirmar efeito atual");
                    break;
                    
                case LED_CMD_SAVE_CONFIG:
                    ESP_LOGI("LED_MANAGER", "Salvando configurações...");
                    vTaskDelay(pdMS_TO_TICKS(100)); // Simula escrita não-volátil
                    ESP_LOGI("LED_MANAGER", "Configurações salvas");
                    break;

                case LED_CMD_CANCEL_CONFIG:
                    ESP_LOGI("LED_MANAGER", "Cancelando configurações...");
                    break;
                    
               case LED_CMD_BUTTON_ERROR:
                    ESP_LOGE("LED_MANAGER", "Erro no botao");
                    break;
               
                    
                default:
                    ESP_LOGE("LED_MANAGER", "Comando desconhecido: %d", cmd.cmd);
                    break;
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Função principal
// -----------------------------------------------------------------------------
void app_main(void) {
	esp_log_level_set("Touch", ESP_LOG_DEBUG);

	// Criação das filas
	button_event_queue =
		xQueueCreate(BUTTON_QUEUE_SIZE, sizeof(button_event_t));
	configASSERT(button_event_queue != NULL);
	ESP_LOGI(TAG, "Button event queue created (size: %d)", BUTTON_QUEUE_SIZE);

	encoder_event_queue =
		xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoder_event_t));
	configASSERT(encoder_event_queue != NULL);
	ESP_LOGI(TAG, "Encoder event queue created (size: %d)", ENCODER_QUEUE_SIZE);

	touch_event_queue = xQueueCreate(TOUCH_QUEUE_SIZE, sizeof(touch_event_t));
	configASSERT(touch_event_queue != NULL);
	ESP_LOGI(TAG, "Touch event queue created (size: %d)", TOUCH_QUEUE_SIZE);

	espnow_event_queue =
		xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
	configASSERT(espnow_event_queue != NULL);
	ESP_LOGI(TAG, "ESP-NOW event queue created (size: %d)", ESPNOW_QUEUE_SIZE);

	UBaseType_t integrated_queue_len = BUTTON_QUEUE_SIZE + ENCODER_QUEUE_SIZE +
									   ESPNOW_QUEUE_SIZE + TOUCH_QUEUE_SIZE;
	integrated_event_queue =
		xQueueCreate(integrated_queue_len, sizeof(integrated_event_t));
	configASSERT(integrated_event_queue != NULL);
	ESP_LOGI(TAG, "Integrated event queue created (size: %lu)",
			 (unsigned long)integrated_queue_len);

	led_cmd_queue = xQueueCreate(LED_CMD_QUEUE_SIZE, sizeof(led_command_t));
	configASSERT(led_cmd_queue != NULL);
	ESP_LOGI(TAG, "LED command queue created (size: %d)", LED_CMD_QUEUE_SIZE);

	// Inicializa botão
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

	// Inicializa encoder
	encoder_config_t enc_cfg = {.pin_a = ENCODER_PIN_A,
								.pin_b = ENCODER_PIN_B,
								.half_step_mode = false,
								.acceleration_enabled = true,
								.accel_gap_ms = ENC_ACCEL_GAP,
								.accel_max_multiplier = MAX_ACCEL_MULTIPLIER};
	encoder_handle_t encoder_handle =
		encoder_create(&enc_cfg, encoder_event_queue);
	configASSERT(encoder_handle != NULL);
	ESP_LOGI(TAG, "Encoder initialized on pins A: %d, B: %d", ENCODER_PIN_A,
			 ENCODER_PIN_B);

	// Inicializa touch
	touch_config_t touch_cfg = {
		.pad = TOUCH_PAD1_PIN,
		.threshold_percent = TOUCH_THRESHOLD_PERCENT,
		.debounce_press_ms = TOUCH_DEBOUNCE_PRESS_MS,
		.debounce_release_ms = TOUCH_DEBOUNCE_RELEASE_MS,
		.hold_time_ms = TOUCH_HOLD_TIME_MS,
		.hold_repeat_interval_ms = TOUCH_HOLD_REPEAT_TIME_MS,
		.recalibration_interval_min = TOUCH_RECALIBRATION_INTERVAL_MIN,
		.enable_hold_repeat = true};
	touch_t *touch_handle = touch_create(&touch_cfg, touch_event_queue);
	configASSERT(touch_handle != NULL);
	ESP_LOGI(TAG, "Touch button initialized on pad %d", TOUCH_PAD1_PIN);

	// Inicializa integrador de inputs
	queue_manager = init_queue_manager(button_event_queue, encoder_event_queue,
									   espnow_event_queue, touch_event_queue,
									   integrated_event_queue);
	configASSERT(queue_manager.queue_set != NULL);
	ESP_LOGI(TAG, "Input integrator initialized.");

	// Inicializa FSM incremental
	fsm_init(integrated_event_queue, led_cmd_queue);

	// Criação das tasks
	BaseType_t task_created;
	task_created = xTaskCreate(integrator_task, "integrator_task",
							   INTEGRATOR_TASK_STACK_SIZE, &queue_manager,
							   INTEGRATOR_TASK_PRIORITY, NULL);
	configASSERT(task_created == pdPASS);

	task_created =
		xTaskCreate(led_manager_task, "led_manager_task", 4096, NULL, 4, NULL);
	configASSERT(task_created == pdPASS);

	ESP_LOGI(TAG, "System initialized. Monitoring events...");
}

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
#include "switch.h"
#include <inttypes.h>
#include <stdio.h>

#include "esp_system.h"
#include "fsm.h" // FSM incremental
#include "led_controller.h"
#include "led_driver.h"
#include "espnow_controller.h"
#include "nvs_flash.h"
#include "nvs_manager.h"
#include "ota_updater.h"
#include "relay_controller.h"

static const char *TAG = "main";

// Filas globais
QueueHandle_t button_event_queue;
QueueHandle_t encoder_event_queue;
QueueHandle_t espnow_event_queue;
QueueHandle_t touch_event_queue;
QueueHandle_t switch_event_queue;
QueueHandle_t integrated_event_queue;
QueueHandle_t led_cmd_queue; // Saída da FSM para o LED Controller
QueueHandle_t led_strip_queue; // Saída do LED Controller para o driver de LED

// -----------------------------------------------------------------------------
// Função principal
// -----------------------------------------------------------------------------
void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Check for OTA mode
    ota_data_t ota_data;
    nvs_manager_load_ota_data(&ota_data);

    if (ota_data.ota_mode_enabled) {
        ESP_LOGI(TAG, "OTA mode enabled. Starting OTA updater...");
        ota_updater_start();
        ESP_LOGI(TAG, "OTA process started. Halting main execution.");
        // Halt main execution while OTA task runs
        while(1) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    ESP_LOGI(TAG, "Normal boot sequence.");

    // Initialize the relay controller (optional, will be a no-op if not used)
    relay_controller_init();

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

    switch_event_queue = xQueueCreate(SWITCH_QUEUE_SIZE, sizeof(switch_event_t));
	configASSERT(switch_event_queue != NULL);
	ESP_LOGI(TAG, "Switch event queue created (size: %d)", SWITCH_QUEUE_SIZE);

	espnow_event_queue =
		xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
	configASSERT(espnow_event_queue != NULL);
	ESP_LOGI(TAG, "ESP-NOW event queue created (size: %d)", ESPNOW_QUEUE_SIZE);

    // Initialize ESP-NOW controller
#if ESP_NOW_ENABLED
    espnow_controller_init(espnow_event_queue);
#endif

	UBaseType_t integrated_queue_len = BUTTON_QUEUE_SIZE + ENCODER_QUEUE_SIZE +
									   ESPNOW_QUEUE_SIZE + TOUCH_QUEUE_SIZE + SWITCH_QUEUE_SIZE;
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

    // Inicializa switch
    switch_config_t switch_cfg = {
        .pin = SWITCH_PIN_1,
        .active_low = true, // Assumes the switch pulls the pin to GND when closed
        .debounce_ms = 50
    };
    switch_t switch_handle = switch_create(&switch_cfg, switch_event_queue);
    configASSERT(switch_handle != NULL);
    ESP_LOGI(TAG, "Switch created on pin %d", SWITCH_PIN_1);

	// Inicializa integrador de inputs
	esp_err_t integrator_status = init_queue_manager(button_event_queue, encoder_event_queue,
									   espnow_event_queue, touch_event_queue,
                                       switch_event_queue,
									   integrated_event_queue);
	configASSERT(integrator_status == ESP_OK);
	ESP_LOGI(TAG, "Input integrator initialized.");

	// Inicializa FSM incremental
	fsm_init(integrated_event_queue, led_cmd_queue);

    // Inicializa o LED Controller real
    led_strip_queue = led_controller_init(led_cmd_queue);
    configASSERT(led_strip_queue != NULL);
    ESP_LOGI(TAG, "Real LED Controller initialized.");

    // Load data from NVS and apply it
    ESP_LOGI(TAG, "Loading configuration from NVS...");
    volatile_data_t v_data;
    static_data_t s_data;
    esp_err_t volatile_err = nvs_manager_load_volatile_data(&v_data);
    esp_err_t static_err = nvs_manager_load_static_data(&s_data);

    // If data was not found, it means defaults were loaded. We should save
    // them now to "heal" the NVS.
    if (volatile_err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Volatile data was not found, saving defaults to NVS.");
        nvs_manager_save_volatile_data(&v_data);
    }
    if (static_err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Static data was not found, saving defaults to NVS.");
        nvs_manager_save_static_data(&s_data);
    }

    led_controller_apply_nvs_data(&v_data, &s_data);

    // Synchronize FSM state with loaded data
    if (v_data.is_on) {
        fsm_set_initial_state(MODE_DISPLAY);
    } else {
        fsm_set_initial_state(MODE_OFF);
    }
    ESP_LOGI(TAG, "NVS configuration loaded and applied.");

    // Inicializa o LED Driver
    led_driver_init(led_strip_queue);
    ESP_LOGI(TAG, "LED Driver initialized.");

	ESP_LOGI(TAG, "System initialized. Monitoring events...");
}

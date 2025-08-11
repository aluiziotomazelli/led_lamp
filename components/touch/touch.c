#include "touch.h"
#include "driver/touch_sensor.h"
#include "portmacro.h"
#include <stdint.h>
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "driver/touch_pad.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "project_config.h"

static const char *TAG = "Touch";

// Internal state machine for the touch button
typedef enum {
	TOUCH_WAIT_FOR_PRESS,
	TOUCH_DEBOUNCE_PRESS,
	TOUCH_WAIT_FOR_RELEASE_OR_HOLD,
	TOUCH_DEBOUNCE_RELEASE
} touch_state_t;

// Internal, complete structure for a touch button instance
struct touch_s {
	touch_pad_t pad;
	touch_state_t state;
	uint32_t press_start_time_ms;
	uint32_t last_time_ms;

	uint16_t threshold_percent;
	uint16_t debounce_press_ms;
	uint16_t debounce_release_ms;
	uint16_t hold_time_ms;

	uint16_t baseline;

	bool hold_generated; // Flag de controle de hold por instância
	bool enable_hold_repeat;
	uint16_t hold_repeat_interval_ms; // Intervalo entre eventos de hold
	uint32_t last_hold_event_ms;	  // Tempo do último evento hold

	uint32_t last_recalibration_ms;
	uint16_t recalibration_interval_ms;
	bool is_recalibrating; // Flag para indicar recalibração em andamento

	bool is_pressed;

	QueueHandle_t output_queue;
	TaskHandle_t task_handle;
};

// Utility to get current time in milliseconds
static uint32_t get_current_time_ms() { return esp_timer_get_time() / 1000; }

// ISR handler to notify the task when a touch event occurs
static void IRAM_ATTR touch_isr_handler(void *arg) {
	touch_t *touch_handle = (touch_t *)arg;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	xTaskNotifyFromISR(touch_handle->task_handle, 1, eSetValueWithOverwrite,
					   &xHigherPriorityTaskWoken);
	ESP_DRAM_LOGD(TAG, "ISR triggered for pad %d", touch_handle->pad);
	if (xHigherPriorityTaskWoken) {
		portYIELD_FROM_ISR();
	}
}

static void touch_recalibrate(touch_t *touch_handle) {
	touch_handle->is_recalibrating = true;
	ESP_LOGI(TAG, "Starting recalibration - pad %d (blocking touch)",
			 touch_handle->pad);

	// Desabilita interrupções durante a recalibração
	touch_pad_intr_disable();

	uint32_t sum = 0;
	const uint8_t samples = 5;
	for (int i = 0; i < samples; i++) {
		uint16_t sample;
		touch_pad_read_raw_data(touch_handle->pad, &sample);
		sum += sample;
		vTaskDelay(pdMS_TO_TICKS(10));
	}

	touch_handle->baseline = sum / samples;
	uint16_t threshold =
		touch_handle->baseline -
		(touch_handle->baseline * touch_handle->threshold_percent / 100);
	touch_pad_set_thresh(touch_handle->pad, threshold);

	// Reabilita interrupções
	touch_pad_intr_enable();

	ESP_LOGI(TAG, "Recalibration complete - pad %d", touch_handle->pad);
	touch_handle->is_recalibrating = false;
}

// Main state machine logic
static touch_event_type_t touch_get_event(touch_t *touch_handle) {
	uint32_t now = get_current_time_ms();

	uint16_t touch_value;
	touch_pad_read_raw_data(touch_handle->pad, &touch_value);

	// Check if the pad is currently "pressed" based on the threshold
	touch_handle->is_pressed =
		(touch_handle->baseline - touch_value) >
		(touch_handle->baseline * touch_handle->threshold_percent / 100);

	switch (touch_handle->state) {
	case TOUCH_WAIT_FOR_PRESS:
		if (touch_handle->is_pressed) {
			touch_handle->press_start_time_ms = now;
			touch_handle->state = TOUCH_DEBOUNCE_PRESS;
			ESP_LOGD("FSM", "DEBOUNCE_PRESS (Pad: %" PRIu32 ")",
					 (uint32_t)touch_handle->pad);
		}
		break;

	case TOUCH_DEBOUNCE_PRESS:
		if (now - touch_handle->press_start_time_ms >
			touch_handle->debounce_press_ms) {
			if (touch_handle->is_pressed) {
				touch_handle->state = TOUCH_WAIT_FOR_RELEASE_OR_HOLD;
				ESP_LOGD("FSM", "WAIT_FOR_RELEASE_OR_HOLD (Pad: %" PRIu32 ")",
						 (uint32_t)touch_handle->pad);
			} else {
				// False press, go back
				touch_handle->state = TOUCH_WAIT_FOR_PRESS;
				ESP_LOGD(
					"FSM",
					"Premature release, back to WAIT_FOR_PRESS (Pad: %" PRIu32
					")",
					(uint32_t)touch_handle->pad);
			}
		}
		break;

	case TOUCH_WAIT_FOR_RELEASE_OR_HOLD:
		if (!touch_handle->is_pressed) {
			uint32_t duration = now - touch_handle->press_start_time_ms;

			if (duration < touch_handle->hold_time_ms) {
				// Toque curto
				touch_handle->last_time_ms = now;
				touch_handle->state = TOUCH_DEBOUNCE_RELEASE;
				touch_handle->hold_generated = false; // Reset explícito
				return TOUCH_PRESS;
			} else {
				// Toque longo já tratado
				touch_handle->state = TOUCH_WAIT_FOR_PRESS;
				touch_handle->hold_generated = false; // Reset explícito
			}
		} else if (now - touch_handle->press_start_time_ms >
				   touch_handle->hold_time_ms) {
			if (!touch_handle->hold_generated) {
				// Primeiro evento de hold
				touch_handle->hold_generated = true;
				touch_handle->last_hold_event_ms = now;
				return TOUCH_HOLD;
			} else if (touch_handle->enable_hold_repeat &&
					   (now - touch_handle->last_hold_event_ms >=
						touch_handle->hold_repeat_interval_ms)) {
				// Eventos repetidos (se habilitado)
				touch_handle->last_hold_event_ms = now;
				return TOUCH_HOLD;
			}
		}
		break;

	case TOUCH_DEBOUNCE_RELEASE:
		if (now - touch_handle->last_time_ms >
			touch_handle->debounce_release_ms) {
			touch_handle->state = TOUCH_WAIT_FOR_PRESS;
			touch_handle->hold_generated = false; // Reseta a flag
			// Não retornamos PRESS aqui, pois já foi retornado na transição
			// anterior
		}
		break;
	}
	//	case TOUCH_DEBOUNCE_RELEASE:
	//		if (now - touch_handle->last_time_ms >
	//			touch_handle->debounce_release_ms) {
	//			if (!touch_handle->is_pressed) {
	//				touch_handle->state = TOUCH_WAIT_FOR_PRESS;
	//				if (!touch_handle
	//						 ->hold_generated) { // Só envia PRESS se não foi
	//HOLD 					return TOUCH_PRESS;
	//				}
	//			} else {
	//				// Press again too soon
	//				touch_handle->state = TOUCH_WAIT_FOR_PRESS;
	//			}
	//			touch_handle->hold_generated = false; // Reseta a flag
	//		}
	//		break;
	//	}
	return TOUCH_NONE;
}

// Individual task for each touch button
static void touch_task(void *param) {
	touch_t *touch_handle = (touch_t *)param;
	const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
	uint32_t last_wake_time = xTaskGetTickCount();

	while (1) {	
		// 1. Verificação periódica de recalibração
		uint32_t now = get_current_time_ms();
		if (!touch_handle->is_pressed &&
			(now - touch_handle->last_recalibration_ms >
			 touch_handle->recalibration_interval_ms)) {
			touch_recalibrate(touch_handle);
			touch_handle->last_recalibration_ms = now;
		}

		// 2. Espera por notificação (ISR ou timeout)
		uint32_t notification_value = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);

		if (notification_value > 0 && !touch_handle->is_recalibrating) {
			ESP_LOGD(TAG, "Processing touch event for pad %d",
					 touch_handle->pad);

			// Processamento do toque
			touch_pad_intr_disable();
			bool event_processed = false;

			do {
				touch_event_type_t event_type = touch_get_event(touch_handle);

				if (event_type != TOUCH_NONE) {
					touch_event_t local_event = {.type = event_type,
												 .pad = touch_handle->pad};

					if (xQueueSend(touch_handle->output_queue, &local_event,
								   pdMS_TO_TICKS(10)) == pdPASS) {
						ESP_LOGI(TAG, "Touch event %d sent for pad %d",
								 event_type, touch_handle->pad);
						event_processed = true;
					} else {
						ESP_LOGW(TAG, "Failed to send event %d for pad %d",
								 event_type, touch_handle->pad);
					}
				}
				vTaskDelay(
					pdMS_TO_TICKS(10)); // Pequeno delay para evitar busy-wait
			} while (!event_processed);

			touch_pad_intr_enable();
		}

		vTaskDelayUntil(&last_wake_time, xMaxBlockTime);
	}
}

// Function to create a new touch button instance
touch_t *touch_create(const touch_config_t *config,
					  QueueHandle_t output_queue) {
	if (!config || !output_queue) {
		ESP_LOGE(TAG, "Invalid arguments: config or output_queue is NULL");
		return NULL;
	}

	touch_t *touch_handle = calloc(1, sizeof(touch_t));
	if (!touch_handle) {
		ESP_LOGE(TAG, "Failed to allocate memory for touch struct");
		return NULL;
	}

	// Copy configuration
	touch_handle->pad = config->pad;
	touch_handle->output_queue = output_queue;
	touch_handle->threshold_percent = config->threshold_percent;
	touch_handle->debounce_press_ms = config->debounce_press_ms;
	touch_handle->debounce_release_ms = config->debounce_release_ms;
	touch_handle->hold_time_ms = config->hold_time_ms;
	touch_handle->hold_repeat_interval_ms = config->hold_repeat_interval_ms;
	touch_handle->last_hold_event_ms = 0;
	touch_handle->recalibration_interval_ms =
		config->recalibration_interval_min * 60 * 1000;
	touch_handle->last_recalibration_ms = get_current_time_ms();
	touch_handle->hold_generated = false;
	touch_handle->enable_hold_repeat = config->enable_hold_repeat;
	touch_handle->state = TOUCH_WAIT_FOR_PRESS;

	// Configure touch pad
	ESP_ERROR_CHECK(touch_pad_init());
	ESP_ERROR_CHECK(touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5,
										  TOUCH_HVOLT_ATTEN_1V));
	ESP_ERROR_CHECK(
		touch_pad_config(touch_handle->pad, 0)); // No threshold initially

	// Set measurement clock and interval, replacing deprecated function
	ESP_ERROR_CHECK(touch_pad_set_measurement_clock_cycles(0xffff));
	ESP_ERROR_CHECK(touch_pad_set_measurement_interval(0xffff));
	ESP_ERROR_CHECK(touch_pad_sw_start());
	ESP_ERROR_CHECK(
		touch_pad_filter_start(10)); // Start filter with a specified period.

	// Get baseline value
	vTaskDelay(pdMS_TO_TICKS(500)); // Delay to allow measurement to stabilize
	touch_pad_read_raw_data(touch_handle->pad, &touch_handle->baseline);
	ESP_LOGI(TAG, "Touch pad %" PRIu32 " baseline: %" PRIu32,
			 (uint32_t)touch_handle->pad, (uint32_t)touch_handle->baseline);

	// Set threshold
	uint16_t threshold =
		touch_handle->baseline -
		(touch_handle->baseline * touch_handle->threshold_percent / 100);
	ESP_ERROR_CHECK(touch_pad_set_thresh(touch_handle->pad, threshold));
	ESP_LOGI(TAG, "Touch pad %" PRIu32 " threshold set to %" PRIu32,
			 (uint32_t)touch_handle->pad, (uint32_t)threshold);

	// Set up interrupt
	ESP_ERROR_CHECK(touch_pad_isr_register(touch_isr_handler, touch_handle));
	ESP_ERROR_CHECK(touch_pad_intr_enable());

	// Create the task
	BaseType_t res =
		xTaskCreate(touch_task, "touch_task", TOUCH_TASK_STACK_SIZE,
					touch_handle, 10, &touch_handle->task_handle);
	if (res != pdPASS) {
		ESP_LOGE(TAG, "Failed to create touch task for pad %" PRIu32,
				 (uint32_t)touch_handle->pad);
		touch_delete(touch_handle);
		return NULL;
	}

	ESP_LOGI(TAG, "Touch button created on pad %" PRIu32,
			 (uint32_t)touch_handle->pad);
	return touch_handle;
}

// Function to delete a touch button instance
void touch_delete(touch_t *touch_handle) {
	if (touch_handle) {
		// Log the deletion before freeing the memory to prevent
		// use-after-free bug
		ESP_LOGI(TAG, "Touch button on pad %" PRIu32 " deleted",
				 (uint32_t)touch_handle->pad);

		if (touch_handle->task_handle) {
			vTaskDelete(touch_handle->task_handle);
		}
		touch_pad_intr_disable();
		touch_pad_isr_deregister(touch_isr_handler, touch_handle);
		free(touch_handle);
	}
}
#pragma once

/**
 * @file fsm.c
 * @brief Implements the main Finite State Machine (FSM) for user interaction.
 *
 * @details This component is the "brain" of the user interface. It receives
 * integrated input events (from buttons, encoders, etc.) from a single queue
 * and, based on its current state, decides what action to take.
 *
 * Its primary responsibility is to manage the application's mode (e.g.,
 * MODE_DISPLAY, MODE_EFFECT_SELECT) and translate user inputs into abstract
 * commands (led_command_t).
 *
 * It does NOT have any knowledge of how the LEDs work, what the effects are,
 * or how to render colors. It is completely decoupled from the led_controller.
 * Communication is done via a command queue (`qOutput`), which sends abstract
 * instructions to be interpreted by the downstream controller. This design
 * improves modularity and separation of concerns.
 */

#include "fsm.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "input_integrator.h"
#include "led_controller.h"
#include "project_config.h"

// Direct includes para acesso às estruturas originais
#include "button.h"
#include "encoder.h"
#include "touch.h"
// #include <cstdint>

#define TIMEOUT_EFFECT_SELECT_MS 10000
#define TIMEOUT_EFFECT_SETUP_MS 15000
#define TIMEOUT_SYSTEM_SETUP_MS 30000

static const char *TAG = "FSM";

static QueueHandle_t qInput;
static QueueHandle_t qOutput;

static fsm_state_t fsm_state = MODE_OFF;
static uint64_t last_event_timestamp_ms = 0;

// Obter timestamp em milissegundos
uint64_t get_current_time_ms() { return esp_timer_get_time() / 1000; }

static bool check_timeout(uint64_t timeout_ms) {
	uint64_t current_ms = get_current_time_ms();
	uint64_t elapsed_ms;

	if (current_ms >= last_event_timestamp_ms) {
		elapsed_ms = current_ms - last_event_timestamp_ms;
	} else {
		// Overflow extremamente raro do contador de 64 bits
		elapsed_ms = (UINT64_MAX - last_event_timestamp_ms) + current_ms + 1;
	}

	return elapsed_ms > timeout_ms;
}

/**
 * @brief Send LED command to output queue
 * @param cmd LED command type
 * @param value Command parameter value
 */
static void send_led_command(led_cmd_type_t cmd, uint32_t timestamp,
							 int16_t value) {
	led_command_t out = {.cmd = cmd, .timestamp = timestamp, .value = value};
	xQueueSend(qOutput, &out, portMAX_DELAY);
}

/**
 * @brief Process button events with direct struct access
 * @param button_evt Button event from integrated queue
 * @return True if event should be processed
 */
static bool process_button_event(const button_event_t *button_evt,
                                uint32_t timestamp) {
    switch (fsm_state) {
        case MODE_OFF:
            switch (button_evt->type) {
                case BUTTON_CLICK:
                    fsm_state = MODE_DISPLAY;
                    send_led_command(LED_CMD_TURN_ON, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_OFF -> MODE_DISPLAY (button click)");
                    return true;
                case BUTTON_LONG_CLICK:
                    fsm_state = MODE_DISPLAY;
                    send_led_command(LED_CMD_TURN_ON_FADE, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_OFF -> MODE_DISPLAY_FADE (long click)");
                    return true;
                default:
                    return false;
            }

        case MODE_DISPLAY:
            switch (button_evt->type) {
                case BUTTON_CLICK:
                    fsm_state = MODE_OFF;
                    send_led_command(LED_CMD_TURN_OFF, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_DISPLAY -> MODE_OFF (button click)");
                    return true;
                case BUTTON_DOUBLE_CLICK:
                    fsm_state = MODE_EFFECT_SELECT;
                    ESP_LOGI(TAG, "MODE_DISPLAY -> MODE_EFFECT_SELECT");
                    return true;
                case BUTTON_LONG_CLICK:
                    fsm_state = MODE_EFFECT_SETUP;
                    ESP_LOGI(TAG, "MODE_DISPLAY -> MODE_EFFECT_SETUP");
                    return true;
                case BUTTON_VERY_LONG_CLICK:
                    fsm_state = MODE_SYSTEM_SETUP;
                    ESP_LOGI(TAG, "MODE_DISPLAY -> MODE_SYSTEM_SETUP");
                    return true;
                default:
                    return false;
            }

        case MODE_EFFECT_SELECT:
            switch (button_evt->type) {
                case BUTTON_CLICK:
                    fsm_state = MODE_DISPLAY;
                    send_led_command(LED_CMD_SET_EFFECT, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_EFFECT_SELECT -> MODE_DISPLAY (effect selected)");
                    return true;
                case BUTTON_DOUBLE_CLICK:
                    send_led_command(LED_CMD_CANCEL_CONFIG, timestamp, 0);
                    fsm_state = MODE_DISPLAY;
                    ESP_LOGI(TAG, "MODE_EFFECT_SELECT -> MODE_DISPLAY (cancelled)");
                    return true;
                case BUTTON_TIMEOUT:
                    fsm_state = MODE_DISPLAY;
                    send_led_command(LED_CMD_SAVE_CONFIG, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_EFFECT_SELECT -> MODE_DISPLAY (timeout)");
                    return true;
                default:
                    return false;
            }

        case MODE_EFFECT_SETUP:
            switch (button_evt->type) {
                case BUTTON_CLICK:
                    send_led_command(LED_CMD_NEXT_EFFECT_PARAM, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_EFFECT_SETUP Next Param");
                    return true;
                case BUTTON_DOUBLE_CLICK:
                    send_led_command(LED_CMD_CANCEL_CONFIG, timestamp, 0);
                    fsm_state = MODE_DISPLAY;
                    ESP_LOGI(TAG, "MODE_EFFECT_SETUP -> MODE_DISPLAY (cancelled)");
                    return true;
                case BUTTON_LONG_CLICK:
                    fsm_state = MODE_DISPLAY;
                    send_led_command(LED_CMD_SAVE_CONFIG, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_EFFECT_SETUP -> MODE_DISPLAY (saved)");
                    return true;
                case BUTTON_TIMEOUT:
                    fsm_state = MODE_DISPLAY;
                    send_led_command(LED_CMD_SAVE_CONFIG, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_EFFECT_SETUP -> MODE_DISPLAY (timeout)");
                    return true;
                default:
                    return false;
            }

        case MODE_SYSTEM_SETUP:
            switch (button_evt->type) {
                case BUTTON_CLICK:
                    send_led_command(LED_CMD_NEXT_SYSTEM_PARAM, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_SYSTEM_SETUP Next Param");
                    return true;
                case BUTTON_DOUBLE_CLICK:
                    send_led_command(LED_CMD_CANCEL_CONFIG, timestamp, 0);
                    fsm_state = MODE_DISPLAY;
                    ESP_LOGI(TAG, "MODE_SYSTEM_SETUP -> MODE_DISPLAY (cancelled)");
                    return true;
                case BUTTON_LONG_CLICK:
                    fsm_state = MODE_DISPLAY;
                    send_led_command(LED_CMD_SAVE_CONFIG, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_SYSTEM_SETUP -> MODE_DISPLAY (saved)");
                    return true;
                case BUTTON_TIMEOUT:
                    fsm_state = MODE_DISPLAY;
                    send_led_command(LED_CMD_SAVE_CONFIG, timestamp, 0);
                    ESP_LOGI(TAG, "MODE_SYSTEM_SETUP -> MODE_DISPLAY (timeout)");
                    return true;
                default:
                    return false;
            }

        default:
            switch (button_evt->type) {
                case BUTTON_NONE_CLICK:
                case BUTTON_ERROR:
                    send_led_command(LED_CMD_BUTTON_ERROR, timestamp, 0);
                    return true;
                default:
                    return false;
            }
    }
}

/**
 * @brief Process encoder events with direct struct access
 * @param encoder_evt Encoder event from integrated queue
 * @return True if event should be processed
 */
static bool process_encoder_event(const encoder_event_t *encoder_evt,
								  uint32_t timestamp) {
	if (encoder_evt->steps == 0) {
		return false;
	}

	int16_t steps = (int16_t)encoder_evt->steps;

	switch (fsm_state) {
	case MODE_DISPLAY:
		send_led_command(LED_CMD_INC_BRIGHTNESS, timestamp, steps);
		ESP_LOGD(TAG, "Brightness adjustment: %d", steps);
		break;

	case MODE_EFFECT_SELECT:
		send_led_command(LED_CMD_INC_EFFECT, timestamp, steps);
		ESP_LOGD(TAG, "Effect selection: %d", steps);
		break;

	case MODE_EFFECT_SETUP:
		send_led_command(LED_CMD_INC_EFFECT_PARAM, timestamp, steps);
		ESP_LOGD(TAG, "Effect parameter adjustment: %d", steps);
		break;

	case MODE_SYSTEM_SETUP:
		send_led_command(LED_CMD_INC_SYSTEM_PARAM, timestamp, steps);
		ESP_LOGD(TAG, "System parameter adjustment: %d", steps);
		break;

	case MODE_OFF:
	default:
		return false;
	}

	return true;
}

/**
 * @brief Process touch events with direct struct access
 * @param touch_evt Touch event from integrated queue
 * @return True if event should be processed
 */
static bool process_touch_event(const touch_event_t *touch_evt,
								uint32_t timestamp) {
	// Convert touch to equivalent button event for consistency
	button_event_t equivalent_button = {.pin = touch_evt->pad,
										.type = (touch_evt->type == TOUCH_PRESS)
													? BUTTON_CLICK
													: BUTTON_LONG_CLICK};

	ESP_LOGD(TAG, "Touch event converted to button equivalent: pin=%d, type=%d",
			 equivalent_button.pin, equivalent_button.type);

	return process_button_event(&equivalent_button, timestamp);
}

/**
 * @brief Process ESPNOW events (placeholder for future implementation)
 * @param espnow_evt ESPNOW event from integrated queue
 * @return True if event should be processed
 */
static bool process_espnow_event(const espnow_event_t *espnow_evt,
								 uint32_t timestamp) {
	ESP_LOGD(TAG,
			 "ESPNOW event received from MAC: "
			 "%02x:%02x:%02x:%02x:%02x:%02x, len=%d",
			 espnow_evt->mac_addr[0], espnow_evt->mac_addr[1],
			 espnow_evt->mac_addr[2], espnow_evt->mac_addr[3],
			 espnow_evt->mac_addr[4], espnow_evt->mac_addr[5],
			 espnow_evt->data_len);

	// TODO: Implement ESPNOW command parsing
	// For now, treat as no-op
	return false;
}

/**
 * @brief Main FSM task function
 * @param pv Unused parameter
 */
static void fsm_task(void *pv) {
	integrated_event_t integrated_evt;
	bool event_processed = false;
	const TickType_t waitTicks = pdMS_TO_TICKS(100);
	if (last_event_timestamp_ms == 0) {
		last_event_timestamp_ms = get_current_time_ms();
	}

	ESP_LOGI(TAG, "FSM task started");

	while (1) {
		if (xQueueReceive(qInput, &integrated_evt, waitTicks) == pdTRUE) {
			event_processed = false;

			// Direct access to original structures - no conversion
			// needed!
			switch (integrated_evt.source) {
			case EVENT_SOURCE_BUTTON:
				event_processed = process_button_event(
					&integrated_evt.data.button, integrated_evt.timestamp);
				break;

			case EVENT_SOURCE_ENCODER:
				event_processed = process_encoder_event(
					&integrated_evt.data.encoder, integrated_evt.timestamp);
				break;

			case EVENT_SOURCE_TOUCH:
				event_processed = process_touch_event(
					&integrated_evt.data.touch, integrated_evt.timestamp);
				break;

			case EVENT_SOURCE_ESPNOW:
				event_processed = process_espnow_event(
					&integrated_evt.data.espnow, integrated_evt.timestamp);
				break;

			default:
				ESP_LOGW(TAG, "Unknown event source: %d",
						 integrated_evt.source);
				break;
			}

			if (event_processed) {
				last_event_timestamp_ms = get_current_time_ms();
			}
		} else {
			// Check for timeout

			if ((fsm_state == MODE_EFFECT_SELECT &&
				 check_timeout(TIMEOUT_EFFECT_SELECT_MS)) ||
				(fsm_state == MODE_EFFECT_SETUP &&
				 check_timeout(TIMEOUT_EFFECT_SETUP_MS)) ||
				(fsm_state == MODE_SYSTEM_SETUP &&
				 check_timeout(TIMEOUT_SYSTEM_SETUP_MS))) {
				fsm_state = MODE_DISPLAY;
				send_led_command(LED_CMD_SAVE_CONFIG, get_current_time_ms(), 0);
				ESP_LOGI(TAG, "Timeout in setup mode -> MODE_DISPLAY "
							  "(auto-save)");
				last_event_timestamp_ms = get_current_time_ms();
			}
		}
	}
}

/**
 * @brief Initialize the FSM module
 * @param inputQueue Queue for integrated input events
 * @param outputQueue Queue for LED commands
 */
void fsm_init(QueueHandle_t inputQueue, QueueHandle_t outputQueue) {
	if (!inputQueue || !outputQueue) {
		ESP_LOGE(TAG, "Invalid queue parameters");
		return;
	}

	qInput = inputQueue;
	qOutput = outputQueue;
	fsm_state = MODE_OFF;
	last_event_timestamp_ms = get_current_time_ms();

	BaseType_t result = xTaskCreate(fsm_task, "FSM", FSM_STACK_SIZE, NULL,
									FSM_TASK_PRIORITY, NULL);
	if (result != pdPASS) {
		ESP_LOGE(TAG, "Failed to create FSM task");
		return;
	}

	ESP_LOGI(TAG, "FSM initialized successfully in state MODE_OFF");
}

/**
 * @brief Get current FSM state (for debugging/monitoring)
 * @return Current FSM state
 */
fsm_state_t fsm_get_state(void) { return fsm_state; }
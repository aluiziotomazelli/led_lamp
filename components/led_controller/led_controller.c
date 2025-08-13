#include "led_controller.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fsm.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LED_CTRL";

// The pixel buffer that holds the current LED colors
static rgb_t *pixel_buffer = NULL;

// State variables
static bool is_on = false;
static uint8_t master_brightness = 100;
static uint8_t current_effect_index = 0;
static uint8_t current_param_index = 0;

// Temporary storage for parameters when in setup mode (for cancel
// functionality)
static effect_param_t *temp_params = NULL;

// Handles for FreeRTOS objects
static QueueHandle_t q_commands_in = NULL;
static QueueHandle_t q_strip_out = NULL;

// External variables from led_effects.c
extern effect_t *effects[];
extern const uint8_t effects_count;

// Forward declaration
static void led_controller_task(void *pv);

/**
 * @brief Applies the master brightness to a single RGB color.
 */
static inline rgb_t apply_brightness(rgb_t color, uint8_t brightness) {
	rgb_t out;
	out.r = (color.r * brightness) / 255;
	out.g = (color.g * brightness) / 255;
	out.b = (color.b * brightness) / 255;
	return out;
}

/**
 * @brief Saves the current parameters of the active effect into a temporary
 * buffer.
 */
static void save_temp_params() {
	effect_t *current_effect = effects[current_effect_index];
	if (temp_params) {
		free(temp_params);
		temp_params = NULL;
	}
	if (current_effect->num_params > 0) {
		temp_params =
			malloc(sizeof(effect_param_t) * current_effect->num_params);
		if (temp_params) {
			memcpy(temp_params, current_effect->params,
				   sizeof(effect_param_t) * current_effect->num_params);
		} else {
			ESP_LOGE(TAG, "Failed to allocate memory for temp params");
		}
	}
}

/**
 * @brief Restores the parameters of the active effect from the temporary
 * buffer.
 */
static void restore_temp_params() {
	effect_t *current_effect = effects[current_effect_index];
	if (temp_params && current_effect->num_params > 0) {
		memcpy(current_effect->params, temp_params,
			   sizeof(effect_param_t) * current_effect->num_params);
		free(temp_params);
		temp_params = NULL;
	}
}

/**
 * @brief Handles an incoming command from the FSM.
 */
static void handle_command(const led_command_t *cmd) {
	effect_t *current_effect = effects[current_effect_index];

	switch (cmd->cmd) {
	case LED_CMD_TURN_ON:
	case LED_CMD_TURN_ON_FADE:
		is_on = true;
		ESP_LOGI(TAG, "LEDs ON");
		break;

	case LED_CMD_TURN_OFF:
		is_on = false;
		ESP_LOGI(TAG, "LEDs OFF");
		break;


        case LED_CMD_INC_BRIGHTNESS: {
            int32_t new_brightness = (int32_t)master_brightness + cmd->value;
            if (new_brightness > 255) {
                master_brightness = 255;
            } else if (new_brightness < 0) {
                master_brightness = 0;
            } else {
                master_brightness = (uint8_t)new_brightness;
            }
            ESP_LOGI(TAG, "Brightness: %d", master_brightness);
            break;
        }

	case LED_CMD_INC_EFFECT: {
		int32_t new_index = current_effect_index + cmd->value;
		if (new_index < 0)
			new_index = effects_count - 1;
		if (new_index >= effects_count)
			new_index = 0;
		current_effect_index = new_index;
		current_param_index = 0; // Reset param index when changing effect
		ESP_LOGI(TAG, "Effect changed to: %s",
				 effects[current_effect_index]->name);
		break;
	}

	case LED_CMD_SET_EFFECT:
		ESP_LOGI(TAG, "Effect set to: %s", current_effect->name);
		save_temp_params(); // Save params when entering effect setup
		break;

	case LED_CMD_INC_EFFECT_PARAM:
		if (current_effect->num_params > 0) {
			effect_param_t *param =
				&current_effect->params[current_param_index];
			param->value += cmd->value * param->step;
			if (param->value > param->max_value)
				param->value = param->max_value;
			if (param->value < param->min_value)
				param->value = param->min_value;
			ESP_LOGI(TAG, "Param '%s' changed to: %d", param->name,
					 param->value);
		}
		break;

	case LED_CMD_NEXT_EFFECT_PARAM:
		if (current_effect->num_params > 0) {
			current_param_index =
				(current_param_index + 1) % current_effect->num_params;
			ESP_LOGI(TAG, "Next param: %s",
					 current_effect->params[current_param_index].name);
		}
		break;

	case LED_CMD_SAVE_CONFIG:
		ESP_LOGI(TAG, "Configuration saved.");
		if (temp_params) {
			free(temp_params);
			temp_params = NULL;
		}
		// Here you would save to NVS
		break;

	case LED_CMD_CANCEL_CONFIG:
		ESP_LOGI(TAG, "Configuration cancelled.");
		restore_temp_params();
		break;

	default:
		// Other commands are ignored by the controller
		break;
	}
}

QueueHandle_t led_controller_init(QueueHandle_t cmd_queue) {
	if (!cmd_queue) {
		ESP_LOGE(TAG, "Command queue is NULL");
		return NULL;
	}
	q_commands_in = cmd_queue;

	pixel_buffer = malloc(sizeof(rgb_t) * NUM_LEDS);
	if (!pixel_buffer) {
		ESP_LOGE(TAG, "Failed to allocate pixel buffer");
		return NULL;
	}

	q_strip_out = xQueueCreate(1, sizeof(led_strip_t));
	if (!q_strip_out) {
		ESP_LOGE(TAG, "Failed to create output queue");
		free(pixel_buffer);
		return NULL;
	}

	BaseType_t result =
		xTaskCreate(led_controller_task, "LED_CTRL_T", LED_CTRL_STACK_SIZE,
					NULL, LED_CTRL_TASK_PRIORITY, NULL);
	if (result != pdPASS) {
		ESP_LOGE(TAG, "Failed to create LED controller task");
		vQueueDelete(q_strip_out);
		free(pixel_buffer);
		return NULL;
	}

	ESP_LOGI(TAG, "LED Controller initialized");
	return q_strip_out;
}

static void led_controller_task(void *pv) {
	led_command_t cmd;
	led_strip_t strip_data = {.pixels = pixel_buffer, .num_pixels = NUM_LEDS};

	TickType_t last_wake_time = xTaskGetTickCount();
	const TickType_t tick_rate = pdMS_TO_TICKS(30); // ~33 FPS

	while (1) {
		// 1. Check for incoming commands (non-blocking)
		if (xQueueReceive(q_commands_in, &cmd, 0) == pdTRUE) {
			handle_command(&cmd);
		}

		// 2. Run the current effect's logic
		if (is_on) {
			effect_t *current_effect = effects[current_effect_index];
			if (current_effect->run) {
				current_effect->run(
					current_effect->params, current_effect->num_params,
					master_brightness, esp_timer_get_time() / 1000,
					pixel_buffer, NUM_LEDS);
			}
			// Apply master brightness
			for (uint16_t i = 0; i < NUM_LEDS; i++) {
				pixel_buffer[i] =
					apply_brightness(pixel_buffer[i], master_brightness);
			}
		} else {
			// If off, just clear the buffer
			memset(pixel_buffer, 0, sizeof(rgb_t) * NUM_LEDS);
		}

		// 3. Send the final pixel buffer to the output queue
		// Use xQueueOverwrite to always send the latest frame
		xQueueOverwrite(q_strip_out, &strip_data);

		// 4. Wait for the next cycle
		vTaskDelayUntil(&last_wake_time, tick_rate);
	}
}

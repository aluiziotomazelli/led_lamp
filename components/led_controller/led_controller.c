#include "led_controller.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fsm.h"
#include "project_config.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LED_CTRL";

// The pixel buffer that holds the current LED colors
static color_t *pixel_buffer = NULL;

// State variables
static bool is_on = false;
static uint8_t master_brightness = 100;
static uint8_t current_effect_index = 0;
static uint8_t current_param_index = 0;
static bool needs_render = true; // Flag to force render

// Temporary storage for parameters when in setup mode (for cancel
// functionality)
static effect_param_t *temp_params = NULL;

// Handles for FreeRTOS objects
static QueueHandle_t q_commands_in = NULL;
static QueueHandle_t q_strip_out = NULL;
static TaskHandle_t render_task_handle = NULL;

// External variables from led_effects.c
extern effect_t *effects[];
extern const uint8_t effects_count;

// Forward declarations
static void led_command_task(void *pv);
static void led_render_task(void *pv);

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
	needs_render = true; // Signal that a change occurred
}

QueueHandle_t led_controller_init(QueueHandle_t cmd_queue) {
	if (!cmd_queue) {
		ESP_LOGE(TAG, "Command queue is NULL");
		return NULL;
	}
	q_commands_in = cmd_queue;

	pixel_buffer = malloc(sizeof(color_t) * NUM_LEDS);
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

	// Create the rendering task
	BaseType_t result = xTaskCreate(
		led_render_task, "LED_RENDER_T", LED_CTRL_STACK_SIZE, NULL,
		LED_CTRL_TASK_PRIORITY, &render_task_handle);
	if (result != pdPASS) {
		ESP_LOGE(TAG, "Failed to create LED render task");
		vQueueDelete(q_strip_out);
		free(pixel_buffer);
		return NULL;
	}

	// Create the command handling task
	result = xTaskCreate(led_command_task, "LED_CMD_T", LED_CTRL_STACK_SIZE,
						 NULL, LED_CTRL_TASK_PRIORITY, NULL);
	if (result != pdPASS) {
		ESP_LOGE(TAG, "Failed to create LED command task");
		vTaskDelete(render_task_handle); // Clean up the other task
		vQueueDelete(q_strip_out);
		free(pixel_buffer);
		return NULL;
	}

	ESP_LOGI(TAG, "LED Controller initialized");
	return q_strip_out;
}

static void led_command_task(void *pv) {
	led_command_t cmd;
	while (1) {
		// Block and wait for a command indefinitely
		if (xQueueReceive(q_commands_in, &cmd, portMAX_DELAY) == pdTRUE) {
			handle_command(&cmd);
			// Notify the render task to wake up and render immediately
			if (render_task_handle) {
				xTaskNotifyGive(render_task_handle);
			}
		}
	}
}

static void led_render_task(void *pv) {
	led_strip_t strip_data = {.pixels = pixel_buffer,
							  .num_pixels = NUM_LEDS,
							  .mode = COLOR_MODE_RGB};
	const TickType_t tick_rate = pdMS_TO_TICKS(30); // ~33 FPS

	while (1) {
		effect_t *current_effect = effects[current_effect_index];
		strip_data.mode = current_effect->color_mode;

		// Determine if we need to re-calculate the effect
		bool should_run_effect = needs_render || current_effect->is_dynamic;

		if (should_run_effect) {
			if (is_on) {
				if (current_effect->run) {
					current_effect->run(current_effect->params,
										current_effect->num_params,
										master_brightness,
										esp_timer_get_time() / 1000,
										pixel_buffer, NUM_LEDS);
				}

				// Apply master brightness
				if (strip_data.mode == COLOR_MODE_HSV) {
					for (uint16_t i = 0; i < NUM_LEDS; i++) {
						uint8_t v = (pixel_buffer[i].hsv.v *
									 master_brightness) /
									255;
						pixel_buffer[i].hsv.v = v;
					}
				} else { // It's RGB
					for (uint16_t i = 0; i < NUM_LEDS; i++) {
						pixel_buffer[i].rgb = apply_brightness(
							pixel_buffer[i].rgb, master_brightness);
					}
				}
			} else {
				// If the strip is off, we just need to render black once
				memset(pixel_buffer, 0, sizeof(color_t) * NUM_LEDS);
				strip_data.mode = COLOR_MODE_RGB;
			}
		}

		// Always reset the flag after checking it for a cycle
		if (needs_render) {
			needs_render = false;
		}

		// Always send the current buffer to the driver
		xQueueOverwrite(q_strip_out, &strip_data);

		// Wait for a notification or timeout
		ulTaskNotifyTake(pdTRUE, tick_rate);
	}
}
#include "led_controller.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "fsm.h"
#include "project_config.h"
#include "hsv2rgb.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "LED_CTRL";

// --- Feedback Animation State ---
typedef enum {
    FEEDBACK_TYPE_NONE,
    FEEDBACK_TYPE_GREEN,
    FEEDBACK_TYPE_RED,
    FEEDBACK_TYPE_BLUE,
    FEEDBACK_TYPE_EFFECT_COLOR,
    FEEDBACK_TYPE_LIMIT,
} feedback_type_t;

static feedback_type_t current_feedback = FEEDBACK_TYPE_NONE;
static uint64_t feedback_start_time_ms = 0;
static uint8_t feedback_blink_count = 0;


// The pixel buffer that holds the current LED colors
static color_t *pixel_buffer = NULL;

// State variables//

// Fade in
static bool is_fading = false;
static uint32_t fade_start_time = 0;
static uint8_t fade_start_brightness = 0;

static bool is_on = false;
static uint8_t master_brightness = 75;
static uint8_t current_effect_index = 0;
static uint8_t current_param_index = 0;
static bool needs_render = true; // Flag to force render

// Temporary storage for parameters when in setup mode (for cancel
// functionality)
static effect_param_t *temp_params = NULL;
static uint8_t temp_effect_index = 255;

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
 * @brief Fills the entire pixel buffer with a solid RGB color.
 */
static void fill_solid_color(rgb_t color) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        pixel_buffer[i].rgb = color;
    }
}

/**
 * @brief Runs the currently active feedback animation.
 * @return true if an animation is active, false otherwise.
 */
static bool run_feedback_animation() {
    if (current_feedback == FEEDBACK_TYPE_NONE) {
        return false;
    }

    uint64_t now = esp_timer_get_time() / 1000;
    uint64_t elapsed = now - feedback_start_time_ms;
    const uint16_t total_duration = 400;

    rgb_t feedback_color = {0, 0, 0};
    uint16_t anim_duration_ms = feedback_blink_count * total_duration; // 125ms on, 125ms off per blink

    // Determine the color for the feedback
    switch (current_feedback) {
        case FEEDBACK_TYPE_GREEN:
            feedback_color = (rgb_t){50, 200, 50};
            break;
        case FEEDBACK_TYPE_RED:
            feedback_color = (rgb_t){200, 50, 50};
            break;
        case FEEDBACK_TYPE_BLUE:
            feedback_color = (rgb_t){40, 40, 200};
            break;
        case FEEDBACK_TYPE_EFFECT_COLOR:
        case FEEDBACK_TYPE_LIMIT: 
            feedback_color = (rgb_t){200, 120, 20};
            break;
//        case FEEDBACK_TYPE_LIMIT: {
//            effect_t* effect = effects[current_effect_index];
//            // Find the Hue parameter to use as base color
//            uint16_t hue = 40;
//            for(int i=0; i < effect->num_params; i++) {
//                if(effect->params[i].type == PARAM_TYPE_HUE) {
//                    hue = effect->params[i].value;
//                    break;
//                }
//            }
//            hsv_to_rgb_spectrum_deg(hue, 255, 255, &feedback_color.r, &feedback_color.g, &feedback_color.b);
//            break;
//        }
        default:
            // Should not happen
            current_feedback = FEEDBACK_TYPE_NONE;
            return false;
    }

    // Check if animation is finished
    if (elapsed >= anim_duration_ms) {
        current_feedback = FEEDBACK_TYPE_NONE;
        return false;
    }

    // Determine if the blink is in the ON or OFF phase
    // Each blink is 250ms long. The first half (0-124ms) is ON.
    bool is_on_phase = (elapsed % total_duration) < (total_duration/2);

    if (is_on_phase) {
        fill_solid_color(apply_brightness(feedback_color, master_brightness));
    } else {
        fill_solid_color((rgb_t){0, 0, 0}); // Off
    }

    return true;
}


/**
 * @brief Handles an incoming command from the FSM.
 */
static void handle_command(const led_command_t *cmd) {
	effect_t *current_effect = effects[current_effect_index];

	// Do not process other commands if a feedback is active, to avoid conflicts
    if (current_feedback != FEEDBACK_TYPE_NONE && cmd->cmd < LED_CMD_FEEDBACK_GREEN) {
        return;
    }

	switch (cmd->cmd) {
	case LED_CMD_TURN_ON:
		is_on = true;
		ESP_LOGI(TAG, "LEDs ON");
		break;
	case LED_CMD_TURN_ON_FADE:
		is_on = true;
		is_fading = true;
		fade_start_time = esp_timer_get_time() / 1000;
		fade_start_brightness = master_brightness;
		master_brightness = 0; // começa do mínimo
		ESP_LOGI(TAG, "LEDs ON with fade");
		break;

	case LED_CMD_TURN_OFF:
		is_on = false;
		ESP_LOGI(TAG, "LEDs OFF");
		break;

	case LED_CMD_INC_BRIGHTNESS: {
		int32_t new_brightness = (int32_t)master_brightness + cmd->value;
		if (new_brightness > 255) {
			master_brightness = 255;
            current_feedback = FEEDBACK_TYPE_LIMIT;
            feedback_start_time_ms = esp_timer_get_time() / 1000;
            feedback_blink_count = 2;
		} else if (new_brightness < MIN_BRIGHTNESS) {
			master_brightness = MIN_BRIGHTNESS;
            current_feedback = FEEDBACK_TYPE_LIMIT;
            feedback_start_time_ms = esp_timer_get_time() / 1000;
            feedback_blink_count = 2;
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
        // This acts as a save/commit of the new effect index
        if (temp_effect_index != 255) {
            temp_effect_index = 255;
        }
		break;

	case LED_CMD_INC_EFFECT_PARAM:
		if (current_effect->num_params > 0) {
			effect_param_t *param =
				&current_effect->params[current_param_index];
			param->value += cmd->value * param->step;

            if (param->is_wrap) {
                if (param->value > param->max_value) {
                    param->value = param->min_value;
                } else if (param->value < param->min_value) {
                    param->value = param->max_value;
                }
            } else {
                if (param->value > param->max_value)
                    param->value = param->max_value;
                if (param->value < param->min_value)
                    param->value = param->min_value;
            }
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
        if (temp_effect_index != 255) {
            temp_effect_index = 255;
        }
		// Here you would save to NVS
		break;

	case LED_CMD_CANCEL_CONFIG:
		ESP_LOGI(TAG, "Configuration cancelled.");
		restore_temp_params();
        if (temp_effect_index != 255) {
            current_effect_index = temp_effect_index;
            temp_effect_index = 255;
        }
		break;

    case LED_CMD_ENTER_EFFECT_SETUP:
        save_temp_params();
        break;

    case LED_CMD_ENTER_EFFECT_SELECT:
        temp_effect_index = current_effect_index;
        break;

    case LED_CMD_FEEDBACK_GREEN:
        current_feedback = FEEDBACK_TYPE_GREEN;
        feedback_start_time_ms = esp_timer_get_time() / 1000;
        feedback_blink_count = 2; // Double blink
        break;

    case LED_CMD_FEEDBACK_RED:
        current_feedback = FEEDBACK_TYPE_RED;
        feedback_start_time_ms = esp_timer_get_time() / 1000;
        feedback_blink_count = 2; // Double blink
        break;

    case LED_CMD_FEEDBACK_BLUE:
        current_feedback = FEEDBACK_TYPE_BLUE;
        feedback_start_time_ms = esp_timer_get_time() / 1000;
        feedback_blink_count = 1; // Single blink
        break;

    case LED_CMD_FEEDBACK_EFFECT_COLOR:
        current_feedback = FEEDBACK_TYPE_EFFECT_COLOR;
        feedback_start_time_ms = esp_timer_get_time() / 1000;
        feedback_blink_count = 1; // Single blink
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
	BaseType_t result =
		xTaskCreate(led_render_task, "LED_RENDER_T", LED_CTRL_STACK_SIZE, NULL,
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
	led_strip_t strip_data = {
		.pixels = pixel_buffer, .num_pixels = NUM_LEDS, .mode = COLOR_MODE_RGB};
	const TickType_t tick_rate =
		pdMS_TO_TICKS(LED_RENDER_INTERVAL_MS); // ~33 FPS
    static bool was_running_feedback = false;

	while (1) {
        bool is_running_feedback = run_feedback_animation();

        if (was_running_feedback && !is_running_feedback) {
            needs_render = true;
        }
        was_running_feedback = is_running_feedback;

        // Prioritize feedback animations over all other rendering
        if (is_running_feedback) {
            strip_data.mode = COLOR_MODE_RGB; // Feedback animations are always RGB
            xQueueOverwrite(q_strip_out, &strip_data);
            // Use a shorter delay for smooth animation, but still allow notifications
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(16)); // ~60 FPS for feedback
            continue; // Skip the rest of the loop
        }

		if (is_fading && is_on) {
			 uint32_t now = esp_timer_get_time() / 1000;
            uint32_t elapsed = now - fade_start_time;
            
            if (elapsed >= FADE_DURATION_MS) {
                // Fade complete
                is_fading = false;
                master_brightness = fade_start_brightness;
            } else {
                // Calculate current brightness during fade
                float progress = (float)elapsed / FADE_DURATION_MS;
                master_brightness = (uint8_t)((fade_start_brightness) * progress);
            }
            needs_render = true; // Force render each frame during fade
		}

		effect_t *current_effect = effects[current_effect_index];
		strip_data.mode = current_effect->color_mode;

		// Determine if we need to re-calculate the effect
		bool should_run_effect = needs_render || current_effect->is_dynamic;

		if (should_run_effect) {
			if (is_on) {
				if (current_effect->run) {
					current_effect->run(
						current_effect->params, current_effect->num_params,
						master_brightness, esp_timer_get_time() / 1000,
						pixel_buffer, NUM_LEDS);
				}

				// Apply master brightness
				if (strip_data.mode == COLOR_MODE_HSV) {
					for (uint16_t i = 0; i < NUM_LEDS; i++) {
						uint8_t v =
							(pixel_buffer[i].hsv.v * master_brightness) / 255;
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
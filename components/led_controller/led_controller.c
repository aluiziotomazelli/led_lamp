/**
 * @file led_controller.c
 * @brief LED Controller implementation with animation support
 * 
 * @details This file implements the high-level LED controller that manages
 *          color modes, animations, and effects for LED strips. It processes
 *          color data and sends it to the hardware driver via queue.
 *          Includes NVS integration, command handling, and system configuration.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Set log level for this module, must come before esp_log.h
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
// ESP-IDF system services
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

// Project specific headers
#include "led_controller.h"
#include "fsm.h"
#include "hsv2rgb.h"
#include "project_config.h"
#include "nvs_manager.h"
#include "ota_updater.h"
#include <string.h>

// Conditional includes
#if ESP_NOW_ENABLED && IS_MASTER
#include "espnow_controller.h"
#endif

static const char *TAG = "LED_CTRL";

//------------------------------------------------------------------------------
// GLOBAL SYSTEM VARIABLES
//------------------------------------------------------------------------------

/// @brief Global minimum brightness setting
static uint8_t g_min_brightness = DEFAULT_MIN_BRIGHTNESS;

/// @brief Global LED offset at beginning of strip
static uint16_t g_led_offset_begin = DEFAULT_LED_OFFSET_BEGIN;

/// @brief Global LED offset at end of strip
static uint16_t g_led_offset_end = DEFAULT_LED_OFFSET_END;

//------------------------------------------------------------------------------
// PRIVATE VARIABLES
//------------------------------------------------------------------------------

/// @brief Feedback animation state enumeration
typedef enum {
    FEEDBACK_TYPE_NONE,          ///< No active feedback animation
    FEEDBACK_TYPE_GREEN,         ///< Green feedback animation
    FEEDBACK_TYPE_RED,           ///< Red feedback animation
    FEEDBACK_TYPE_BLUE,          ///< Blue feedback animation
    FEEDBACK_TYPE_EFFECT_COLOR,  ///< Effect color feedback animation
    FEEDBACK_TYPE_LIMIT,         ///< Limit feedback animation
} feedback_type_t;

/// @brief Current feedback animation type
static feedback_type_t current_feedback = FEEDBACK_TYPE_NONE;

/// @brief Feedback animation start time in milliseconds
static uint64_t feedback_start_time_ms = 0;

/// @brief Number of feedback animation blinks
static uint8_t feedback_blink_count = 0;

/// @brief Pixel buffer for current LED colors
static color_t *pixel_buffer = NULL;

/// @brief Strip mode state - current LED offset
static uint16_t led_offset = 0;

/// @brief Strip mode state - number of active LEDs
static uint16_t active_num_leds = NUM_LEDS;

/// @brief Power state of LEDs
static bool is_on = false;

/// @brief Target master brightness (0-255)
static uint8_t master_brightness = 75;

/// @brief Instantaneous brightness for rendering (0-255)
static uint8_t current_brightness = 0;

/// @brief Current effect index
static uint8_t current_effect_index = 0;

/// @brief Current parameter index
static uint8_t current_param_index = 0;

/// @brief Flag to force render
static bool needs_render = true;

/// @brief Temporary storage for parameters in setup mode
static effect_param_t *temp_params = NULL;

/// @brief Temporary effect index storage
static uint8_t temp_effect_index = 255;

/// @brief System parameter enumeration for setup mode
typedef enum {
    SYS_PARAM_OFFSET_BEGIN,      ///< LED offset at beginning parameter
    SYS_PARAM_OFFSET_END,        ///< LED offset at end parameter
    SYS_PARAM_MIN_BRIGHTNESS,    ///< Minimum brightness parameter
    SYS_PARAM_COUNT              ///< Total number of system parameters
} system_param_t;

/// @brief Current system parameter being edited
static system_param_t current_sys_param = SYS_PARAM_OFFSET_BEGIN;

/// @brief Temporary offset begin value
static uint16_t temp_offset_begin = 0;

/// @brief Temporary offset end value
static uint16_t temp_offset_end = 0;

/// @brief Temporary minimum brightness value
static uint8_t temp_min_brightness = 0;

/// @brief Input command queue handle
static QueueHandle_t q_commands_in = NULL;

/// @brief Output strip data queue handle
static QueueHandle_t q_strip_out = NULL;

/// @brief Render task handle
static TaskHandle_t render_task_handle = NULL;

/// @brief Brightness save timer handle
static TimerHandle_t brightness_save_timer = NULL;

/// @brief External reference to effects array from led_effects.c
extern effect_t *effects[];

/// @brief External reference to effects count from led_effects.c
extern const uint8_t effects_count;

//------------------------------------------------------------------------------
// PRIVATE FUNCTION DECLARATIONS
//------------------------------------------------------------------------------

/**
 * @brief LED command task main function
 * 
 * @param pv Task parameters (unused)
 * 
 * @note Handles incoming commands from the FSM
 */
static void led_command_task(void *pv);

/**
 * @brief LED render task main function
 * 
 * @param pv Task parameters (unused)
 * 
 * @note Handles rendering of LED effects and animations
 */
static void led_render_task(void *pv);

/**
 * @brief Trigger volatile data save to NVS
 */
static void trigger_volatile_save(void);

/**
 * @brief Trigger static data save to NVS
 */
static void trigger_static_save(void);

/**
 * @brief Brightness save timer callback
 * 
 * @param xTimer Timer handle
 */
static void brightness_save_callback(TimerHandle_t xTimer);

/**
 * @brief Apply brightness to RGB color
 * 
 * @param color Input RGB color
 * @param brightness Brightness level (0-255)
 * @return Adjusted RGB color
 */
static inline rgb_t apply_brightness(rgb_t color, uint8_t brightness);

/**
 * @brief Save current parameters to temporary buffer
 */
static void save_temp_params(void);

/**
 * @brief Restore parameters from temporary buffer
 */
static void restore_temp_params(void);

/**
 * @brief Fill buffer with solid color
 * 
 * @param color RGB color to fill with
 */
static void fill_solid_color(rgb_t color);

/**
 * @brief Run feedback animation
 * 
 * @return true if animation is active, false otherwise
 */
static bool run_feedback_animation(void);

#if ESP_NOW_ENABLED && IS_MASTER
/**
 * @brief Send ESP-NOW command
 * 
 * @param cmd Command to send
 */
static void send_espnow_command(const led_command_t *cmd);
#endif

/**
 * @brief Handle incoming command
 * 
 * @param cmd Command to process
 */
static void handle_command(const led_command_t *cmd);

//------------------------------------------------------------------------------
// PRIVATE FUNCTION IMPLEMENTATIONS
//------------------------------------------------------------------------------

/**
 * @brief Apply brightness to RGB color
 */
static inline rgb_t apply_brightness(rgb_t color, uint8_t brightness) {
    rgb_t out;
    out.r = (color.r * brightness) / 255;
    out.g = (color.g * brightness) / 255;
    out.b = (color.b * brightness) / 255;
    return out;
}

/**
 * @brief Save current parameters to temporary buffer
 */
static void save_temp_params() {
    effect_t *current_effect = effects[current_effect_index];
    if (temp_params) {
        free(temp_params);
        temp_params = NULL;
    }
    if (current_effect->num_params > 0) {
        temp_params = malloc(sizeof(effect_param_t) * current_effect->num_params);
        if (temp_params) {
            memcpy(temp_params, current_effect->params,
                   sizeof(effect_param_t) * current_effect->num_params);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for temp params");
        }
    }
}

/**
 * @brief Restore parameters from temporary buffer
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
 * @brief Fill buffer with solid color
 */
static void fill_solid_color(rgb_t color) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        pixel_buffer[i].rgb = color;
    }
}

/**
 * @brief Run feedback animation
 */
static bool run_feedback_animation() {
    if (current_feedback == FEEDBACK_TYPE_NONE) {
        return false;
    }

    uint64_t now = esp_timer_get_time() / 1000;
    uint64_t elapsed = now - feedback_start_time_ms;
    const uint16_t total_duration = 400;

    rgb_t feedback_color = {0, 0, 0};
    uint16_t anim_duration_ms = feedback_blink_count * total_duration;

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
        feedback_color = (rgb_t){150, 100, 20};
        break;
    default:
        current_feedback = FEEDBACK_TYPE_NONE;
        return false;
    }

    // Check if animation is finished
    if (elapsed >= anim_duration_ms) {
        current_feedback = FEEDBACK_TYPE_NONE;
        return false;
    }

    // Determine if the blink is in the ON or OFF phase
    bool is_on_phase = (elapsed % total_duration) < (total_duration / 2);

    if (is_on_phase) {
        fill_solid_color(apply_brightness(feedback_color, master_brightness));
    } else {
        fill_solid_color((rgb_t){0, 0, 0}); // Off
    }

    return true;
}

#if ESP_NOW_ENABLED && IS_MASTER
/**
 * @brief Send ESP-NOW command
 */
static void send_espnow_command(const led_command_t *cmd) {
    espnow_message_t msg = {.cmd = *cmd};
    espnow_controller_send(&msg);
}
#endif

/**
 * @brief Handle incoming command
 */
static void handle_command(const led_command_t *cmd) {
    effect_t *current_effect = effects[current_effect_index];

    // Do not process other commands if a feedback is active, to avoid conflicts
    if (current_feedback != FEEDBACK_TYPE_NONE &&
        cmd->cmd < LED_CMD_FEEDBACK_GREEN) {
        return;
    }

    switch (cmd->cmd) {
    case LED_CMD_TURN_ON:
        is_on = true;
        ESP_LOGI(TAG, "LEDs ON");
        trigger_volatile_save();
#if ESP_NOW_ENABLED && IS_MASTER
        send_espnow_command(cmd);
#endif
        break;

    case LED_CMD_TURN_OFF:
        is_on = false;
        ESP_LOGI(TAG, "LEDs OFF");
        trigger_volatile_save();
#if ESP_NOW_ENABLED && IS_MASTER
        send_espnow_command(cmd);
#endif
        break;

    case LED_CMD_SET_EFFECT:
        if (cmd->value >= 0 && cmd->value < effects_count) {
            current_effect_index = (uint8_t)cmd->value;
            current_param_index = 0; // Reset param index
            ESP_LOGI(TAG, "Effect set to index: %d (%s)",
                     current_effect_index,
                     effects[current_effect_index]->name);
            trigger_volatile_save();
#if ESP_NOW_ENABLED && IS_MASTER
            send_espnow_command(cmd);
#endif
        }
        break;

    case LED_CMD_SET_BRIGHTNESS:
        if (cmd->value >= g_min_brightness && cmd->value <= 255) {
            master_brightness = (uint8_t)cmd->value;
            ESP_LOGI(TAG, "Brightness set to: %d", master_brightness);
            // Use the timer to save after a delay, reducing NVS writes
            if (brightness_save_timer != NULL) {
                xTimerReset(brightness_save_timer, portMAX_DELAY);
            }
#if ESP_NOW_ENABLED && IS_MASTER
            send_espnow_command(cmd);
#endif
        }
        break;

    case LED_CMD_SET_EFFECT_PARAM: {
        uint8_t param_idx = cmd->param_idx;
        int16_t param_val = cmd->value;

        if (current_effect->num_params > 0 &&
            param_idx < current_effect->num_params) {
            effect_param_t *param = &current_effect->params[param_idx];

            // Clamp value to its defined min/max
            if (param_val > param->max_value) {
                param->value = param->max_value;
            } else if (param_val < param->min_value) {
                param->value = param->min_value;
            } else {
                param->value = param_val;
            }
            ESP_LOGI(TAG, "Param '%s' (#%d) set to: %d", param->name,
                     param_idx, param->value);
#if ESP_NOW_ENABLED && IS_MASTER
            send_espnow_command(cmd);
#endif
        }
        break;
    }

    case LED_CMD_SET_STRIP_MODE:
        if (cmd->value == 1) { // Restricted mode (from switch open)
            led_offset = g_led_offset_begin;
            active_num_leds = NUM_LEDS - (g_led_offset_begin + g_led_offset_end);
        } else { // Full mode (from switch closed)
            led_offset = 0;
            active_num_leds = NUM_LEDS;
        }
        ESP_LOGI(TAG, "Strip mode set. Offset: %d, Active LEDs: %d", led_offset,
                 active_num_leds);
#if ESP_NOW_ENABLED && IS_MASTER
        send_espnow_command(cmd);
#endif
        break;

    case LED_CMD_NEXT_EFFECT_PARAM:
        if (current_effect->num_params > 0) {
            current_param_index = (current_param_index + 1) % current_effect->num_params;
            ESP_LOGI(TAG, "Next param: %s",
                     current_effect->params[current_param_index].name);
#if ESP_NOW_ENABLED && IS_MASTER
            send_espnow_command(cmd);
#endif
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
        // When effect setup is saved, it's a good time to save all static parameters
        trigger_static_save();
        break;

    case LED_CMD_SYNC_AND_SAVE_STATIC_CONFIG:
        ESP_LOGI(TAG, "Syncing and saving static config to slaves.");
        trigger_static_save(); // Save locally
#if ESP_NOW_ENABLED && IS_MASTER
        send_espnow_command(cmd); // Broadcast to slaves
#endif
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
    case LED_CMD_FEEDBACK_RED:
    case LED_CMD_FEEDBACK_BLUE:
    case LED_CMD_FEEDBACK_EFFECT_COLOR:
    case LED_CMD_FEEDBACK_LIMIT:
#if IS_SLAVE && !SLAVE_ENABLE_FEEDBACK
        // Do nothing on slave if feedback is disabled
#else
        if (cmd->cmd == LED_CMD_FEEDBACK_GREEN) {
            current_feedback = FEEDBACK_TYPE_GREEN;
            feedback_blink_count = 2;
        } else if (cmd->cmd == LED_CMD_FEEDBACK_RED) {
            current_feedback = FEEDBACK_TYPE_RED;
            feedback_blink_count = 2;
        } else if (cmd->cmd == LED_CMD_FEEDBACK_BLUE) {
            current_feedback = FEEDBACK_TYPE_BLUE;
            feedback_blink_count = 1;
        } else if (cmd->cmd == LED_CMD_FEEDBACK_EFFECT_COLOR) {
            current_feedback = FEEDBACK_TYPE_EFFECT_COLOR;
            feedback_blink_count = 1;
        } else if (cmd->cmd == LED_CMD_FEEDBACK_LIMIT) {
            current_feedback = FEEDBACK_TYPE_LIMIT;
            feedback_blink_count = 2;
        }
        feedback_start_time_ms = esp_timer_get_time() / 1000;
#endif

        // Master always broadcasts the feedback command
#if ESP_NOW_ENABLED && IS_MASTER
        send_espnow_command(cmd);
#endif
        break;

    default:
        // Other commands are ignored by the controller
        break;
    }
    needs_render = true; // Signal that a change occurred
}

//------------------------------------------------------------------------------
// NVS INTEGRATION FUNCTIONS
//------------------------------------------------------------------------------

/**
 * @brief Apply NVS data to controller state
 */
void led_controller_apply_nvs_data(const volatile_data_t *v_data, const static_data_t *s_data) {
    ESP_LOGI(TAG, "Applying loaded NVS data to controller state.");

    // Apply volatile data
    is_on = v_data->is_on;
    master_brightness = v_data->master_brightness;
    current_effect_index = v_data->effect_index;
    if (current_effect_index >= effects_count) {
        ESP_LOGW(TAG, "Saved effect index %d is out of bounds, resetting to 0.", current_effect_index);
        current_effect_index = 0;
    }

    // Apply static data
    g_min_brightness = s_data->min_brightness;
    g_led_offset_begin = s_data->led_offset_begin;
    g_led_offset_end = s_data->led_offset_end;

    // Apply effect parameters
    for (uint8_t i = 0; i < effects_count && i < NVS_NUM_EFFECTS; i++) {
        for (uint8_t j = 0; j < effects[i]->num_params && j < NVS_MAX_PARAMS_PER_EFFECT; j++) {
            effects[i]->params[j].value = s_data->effect_params[i][j];
        }
    }

    // Update live render variables from newly loaded offsets
    led_offset = g_led_offset_begin;
    active_num_leds = NUM_LEDS - (g_led_offset_begin + g_led_offset_end);

    needs_render = true;
}

/**
 * @brief Trigger volatile data save
 */
static void trigger_volatile_save(void) {
    volatile_data_t v_data;
    v_data.is_on = is_on;
    v_data.master_brightness = master_brightness;
    v_data.effect_index = current_effect_index;
    if (nvs_manager_save_volatile_data(&v_data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save volatile data");
    }
}

/**
 * @brief Brightness save timer callback
 */
static void brightness_save_callback(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Brightness stable for 10s, saving volatile data.");
    trigger_volatile_save();
}

/**
 * @brief Trigger static data save
 */
static void trigger_static_save(void) {
    static_data_t s_data;
    s_data.min_brightness = g_min_brightness;
    s_data.led_offset_begin = g_led_offset_begin;
    s_data.led_offset_end = g_led_offset_end;

    for (uint8_t i = 0; i < effects_count && i < NVS_NUM_EFFECTS; i++) {
        for (uint8_t j = 0; j < effects[i]->num_params && j < NVS_MAX_PARAMS_PER_EFFECT; j++) {
            s_data.effect_params[i][j] = effects[i]->params[j].value;
        }
    }

    if (nvs_manager_save_static_data(&s_data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save static data");
    }
}

//------------------------------------------------------------------------------
// PUBLIC FUNCTION IMPLEMENTATIONS
//------------------------------------------------------------------------------

/**
 * @brief Initialize LED controller
 */
QueueHandle_t led_controller_init(QueueHandle_t cmd_queue) {
    if (!cmd_queue) {
        ESP_LOGE(TAG, "Command queue is NULL");
        return NULL;
    }
    q_commands_in = cmd_queue;

    // Create the timer for saving brightness after a delay
    brightness_save_timer = xTimerCreate("BrightnessTimer", pdMS_TO_TICKS(10000), pdFALSE, (void *)0, brightness_save_callback);
    if (brightness_save_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create brightness save timer");
        // Continue without this functionality
    }

    pixel_buffer = malloc(sizeof(color_t) * NUM_LEDS);
    if (!pixel_buffer) {
        ESP_LOGE(TAG, "Failed to allocate pixel buffer");
        return NULL;
    }

    q_strip_out = xQueueCreate(LED_STRIP_QUEUE_SIZE, sizeof(led_strip_t));
    if (!q_strip_out) {
        ESP_LOGE(TAG, "Failed to create output queue");
        free(pixel_buffer);
        return NULL;
    }

    // Create the rendering task
    BaseType_t result = xTaskCreate(led_render_task, "LED_RENDER_T", LED_RENDER_STACK_SIZE,
                                   NULL, LED_RENDER_TASK_PRIORITY, &render_task_handle);
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

/**
 * @brief LED command task main function
 */
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

/**
 * @brief LED render task main function
 */
static void led_render_task(void *pv) {
    led_strip_t strip_data = {
        .pixels = pixel_buffer, .num_pixels = NUM_LEDS, .mode = COLOR_MODE_RGB};
    const TickType_t tick_rate = pdMS_TO_TICKS(LED_RENDER_INTERVAL_MS); // ~33 FPS
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

        // Stateless fade logic: gradually move current_brightness to its target
        uint8_t target_brightness = is_on ? master_brightness : 0;
        if (current_brightness != target_brightness) {
            if (current_brightness < target_brightness) {
                current_brightness++;
            } else {
                current_brightness--;
            }
            needs_render = true;
        }

        effect_t *current_effect = effects[current_effect_index];
        strip_data.mode = current_effect->color_mode;

        // Determine if we need to re-calculate the effect
        bool should_run_effect = needs_render || current_effect->is_dynamic;

        if (should_run_effect) {
            if (current_brightness > 0) {
                // Clear the entire buffer to black first
                memset(pixel_buffer, 0, sizeof(color_t) * NUM_LEDS);

                if (current_effect->run) {
                    // Define the active region for the effect
                    color_t *effect_buffer = pixel_buffer + led_offset;
                    current_effect->run(current_effect->params,
                                      current_effect->num_params,
                                      current_brightness,
                                      esp_timer_get_time() / 1000,
                                      effect_buffer, active_num_leds);
                }

                // Apply brightness
                if (strip_data.mode == COLOR_MODE_HSV) {
                    for (uint16_t i = 0; i < NUM_LEDS; i++) {
                        uint8_t v = (pixel_buffer[i].hsv.v * current_brightness) / 255;
                        pixel_buffer[i].hsv.v = v;
                    }
                } else { // It's RGB
                    for (uint16_t i = 0; i < NUM_LEDS; i++) {
                        pixel_buffer[i].rgb = apply_brightness(pixel_buffer[i].rgb, current_brightness);
                    }
                }
            } else {
                // If the strip is off or brightness is 0, render black once
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

//------------------------------------------------------------------------------
// SYSTEM SETUP FUNCTIONS
//------------------------------------------------------------------------------

/**
 * @brief Enter system setup mode
 */
void led_controller_enter_system_setup(void) {
    // Copy current global values to temporary variables for editing
    temp_offset_begin = g_led_offset_begin;
    temp_offset_end = g_led_offset_end;
    temp_min_brightness = g_min_brightness;
    current_sys_param = SYS_PARAM_OFFSET_BEGIN;
    ESP_LOGI(TAG, "Entering system setup.");
}

/**
 * @brief Save system configuration
 */
void led_controller_save_system_config(void) {
    // Copy temporary values to global variables
    g_led_offset_begin = temp_offset_begin;
    g_led_offset_end = temp_offset_end;
    g_min_brightness = temp_min_brightness;
    ESP_LOGI(TAG, "System config saved. Offsets: %d/%d, Min Brightness: %d",
             g_led_offset_begin, g_led_offset_end, g_min_brightness);

    trigger_static_save();
}

/**
 * @brief Cancel system configuration changes
 */
void led_controller_cancel_system_config(void) {
    // Discard temporary values and revert any live preview
    led_offset = g_led_offset_begin;
    active_num_leds = NUM_LEDS - (g_led_offset_begin + g_led_offset_end);
    ESP_LOGI(TAG, "System config cancelled.");
    needs_render = true;
    if (render_task_handle) {
        xTaskNotifyGive(render_task_handle);
    }
}

/**
 * @brief Select next system parameter
 */
void led_controller_next_system_param(void) {
    current_sys_param = (system_param_t)((current_sys_param + 1) % SYS_PARAM_COUNT);
    ESP_LOGI(TAG, "Next system param: %d", current_sys_param);
}

/**
 * @brief Increment/decrement current system parameter value
 * 
 * @param steps Number of steps to adjust (positive or negative)
 * @param limit_hit Pointer to boolean indicating if min/max limit was reached
 */
void led_controller_inc_system_param(int16_t steps, bool *limit_hit) {
    if (limit_hit)
        *limit_hit = false;

    switch (current_sys_param) {
    case SYS_PARAM_OFFSET_BEGIN: {
        int32_t new_offset = (int32_t)temp_offset_begin + steps;
        if (new_offset < 0) {
            new_offset = 0;
            if (limit_hit)
                *limit_hit = true;
        }
        if (new_offset + temp_offset_end >= NUM_LEDS) {
            new_offset = NUM_LEDS - temp_offset_end - 1;
            if (limit_hit)
                *limit_hit = true;
        }
        temp_offset_begin = (uint16_t)new_offset;
        ESP_LOGI(TAG, "Temp offset begin: %d", temp_offset_begin);
        break;
    }
    case SYS_PARAM_OFFSET_END: {
        int32_t new_offset = (int32_t)temp_offset_end + steps;
        if (new_offset < 0) {
            new_offset = 0;
            if (limit_hit)
                *limit_hit = true;
        }
        if (temp_offset_begin + new_offset >= NUM_LEDS) {
            new_offset = NUM_LEDS - temp_offset_begin - 1;
            if (limit_hit)
                *limit_hit = true;
        }
        temp_offset_end = (uint16_t)new_offset;
        ESP_LOGI(TAG, "Temp offset end: %d", temp_offset_end);
        break;
    }
    case SYS_PARAM_MIN_BRIGHTNESS: {
        int32_t new_brightness = (int32_t)temp_min_brightness + steps;
        if (new_brightness < 0) {
            new_brightness = 0;
            if (limit_hit)
                *limit_hit = true;
        }
        if (new_brightness > 255) {
            new_brightness = 255;
            if (limit_hit)
                *limit_hit = true;
        }
        temp_min_brightness = (uint8_t)new_brightness;
        ESP_LOGI(TAG, "Temp min brightness: %d", temp_min_brightness);
        break;
    }
    default:
        break;
    }

    // Live preview for offsets
    led_offset = temp_offset_begin;
    active_num_leds = NUM_LEDS - (temp_offset_begin + temp_offset_end);

    needs_render = true;
    if (render_task_handle) {
        xTaskNotifyGive(render_task_handle);
    }
}

/**
 * @brief Restore current effect parameters to default values
 */
void led_controller_restore_current_effect_defaults(void) {
    effect_t *effect = effects[current_effect_index];
    ESP_LOGI(TAG, "Restoring parameters for effect '%s' to default.", effect->name);

    for (int i = 0; i < effect->num_params; i++) {
        effect->params[i].value = effect->params[i].default_value;
    }

    needs_render = true;
    if (render_task_handle) {
        xTaskNotifyGive(render_task_handle);
    }
}

/**
 * @brief Perform factory reset of all system and effect settings
 */
void led_controller_factory_reset(void) {
    ESP_LOGI(TAG, "Performing factory reset.");

    // Reset system parameters
    g_min_brightness = DEFAULT_MIN_BRIGHTNESS;
    g_led_offset_begin = DEFAULT_LED_OFFSET_BEGIN;
    g_led_offset_end = DEFAULT_LED_OFFSET_END;

    // Also update live render variables
    led_offset = g_led_offset_begin;
    active_num_leds = NUM_LEDS - (g_led_offset_begin + g_led_offset_end);

    // Reset all effect parameters
    for (int i = 0; i < effects_count; i++) {
        effect_t *effect = effects[i];
        for (int j = 0; j < effect->num_params; j++) {
            effect->params[j].value = effect->params[j].default_value;
        }
    }

    // Persist the factory state to NVS
    trigger_static_save();
    trigger_volatile_save(); // Also save volatile state like brightness/effect index

    needs_render = true;
    if (render_task_handle) {
        xTaskNotifyGive(render_task_handle);
    }
}

//------------------------------------------------------------------------------
// GETTER FUNCTIONS
//------------------------------------------------------------------------------

/**
 * @brief Get current power state of LEDs
 * 
 * @return true if LEDs are on, false if off
 */
bool led_controller_is_on(void) {
    return is_on;
}

/**
 * @brief Get current master brightness level
 * 
 * @return Brightness value (0-255)
 */
uint8_t led_controller_get_brightness(void) {
    return master_brightness;
}

/**
 * @brief Get current effect index
 * 
 * @return Index of currently active effect
 */
uint8_t led_controller_get_effect_index(void) {
    return current_effect_index;
}

/**
 * @brief Get current effect parameter index
 *
 * @return Index of currently active effect parameter
 */
uint8_t led_controller_get_current_param_index(void) {
	return current_param_index;
}

/**
 * @brief Get parameters for current effect
 * 
 * @param num_params Pointer to store number of parameters
 * @return Pointer to effect parameters array
 */
effect_param_t* led_controller_get_effect_params(uint8_t *num_params) {
    if (num_params) {
        *num_params = effects[current_effect_index]->num_params;
    }
    return effects[current_effect_index]->params;
}

//------------------------------------------------------------------------------
// STATE MODIFIER FUNCTIONS
//------------------------------------------------------------------------------

/**
 * @brief Increment/decrement brightness level
 * 
 * @param steps Number of steps to adjust (positive or negative)
 * @param limit_hit Pointer to boolean indicating if min/max limit was reached
 * @return New brightness value
 */
uint8_t led_controller_inc_brightness(int16_t steps, bool *limit_hit) {
    if (limit_hit) *limit_hit = false;
    int32_t new_brightness = (int32_t)master_brightness + steps;

    if (new_brightness > 255) {
        master_brightness = 255;
        if (limit_hit) *limit_hit = true;
    } else if (new_brightness < g_min_brightness) {
        master_brightness = g_min_brightness;
        if (limit_hit) *limit_hit = true;
    } else {
        master_brightness = (uint8_t)new_brightness;
    }

    needs_render = true;
    if (render_task_handle) {
        xTaskNotifyGive(render_task_handle);
    }
    return master_brightness;
}

/**
 * @brief Cycle through available effects
 * 
 * @param steps Number of effects to advance (positive or negative)
 * @return New effect index
 */
uint8_t led_controller_inc_effect(int16_t steps) {
    int32_t new_index = current_effect_index + steps;
    if (new_index < 0) {
        new_index = effects_count - 1;
    }
    if (new_index >= effects_count) {
        new_index = 0;
    }
    current_effect_index = new_index;
    current_param_index = 0; // Reset param index when changing effect
    needs_render = true;
    if (render_task_handle) {
        xTaskNotifyGive(render_task_handle);
    }
    return current_effect_index;
}

/**
 * @brief Adjust current effect parameter value
 * 
 * @param steps Number of steps to adjust (positive or negative)
 * @param limit_hit Pointer to boolean indicating if min/max limit was reached
 * @return Packed value containing parameter index and new value
 */
int16_t led_controller_inc_effect_param(int16_t steps, bool *limit_hit) {
    if (limit_hit) *limit_hit = false;
    effect_t *current_effect = effects[current_effect_index];

    if (current_effect->num_params > 0) {
        effect_param_t *param = &current_effect->params[current_param_index];
        int32_t new_value = (int32_t)param->value + (steps * param->step);

        if (param->is_wrap) {
            if (new_value > param->max_value) {
                param->value = param->min_value;
            } else if (new_value < param->min_value) {
                param->value = param->max_value;
            } else {
                param->value = (int16_t)new_value;
            }
        } else {
            if (new_value > param->max_value) {
                param->value = param->max_value;
                if (limit_hit) *limit_hit = true;
            } else if (new_value < param->min_value) {
                param->value = param->min_value;
                if (limit_hit) *limit_hit = true;
            } else {
                param->value = (int16_t)new_value;
            }
        }
        needs_render = true;
        if (render_task_handle) {
            xTaskNotifyGive(render_task_handle);
        }
        return param->value;
    }

    // Fallback
    return 0;
}
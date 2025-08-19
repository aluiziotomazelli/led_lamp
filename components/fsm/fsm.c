/**
 * @file fsm.c
 * @brief Finite State Machine implementation for LED control system
 * 
 * @details This file implements the main state machine that processes input events
 * from various sources and generates LED commands based on the current system state.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <stdint.h>

// Set log level for this module, must come before esp_log.h
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
// ESP-IDF system services
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Input device drivers
#include "button.h"
#include "encoder.h"
#include "switch.h"
#include "touch.h"

// Project components
#include "espnow_controller.h"
#include "input_integrator.h"
#include "led_controller.h"

// Project specific headers
#include "fsm.h"
#include "project_config.h"

static const char *TAG = "FSM";

// Timeout configurations for different states
#define TIMEOUT_EFFECT_SELECT_MS 10000   ///< Timeout for effect selection mode
#define TIMEOUT_EFFECT_SETUP_MS 15000    ///< Timeout for effect setup mode
#define TIMEOUT_SYSTEM_SETUP_MS 30000    ///< Timeout for system setup mode

// Static variables
static QueueHandle_t qInput = NULL;      ///< Input event queue
static QueueHandle_t qOutput = NULL;     ///< Output command queue
static fsm_state_t fsm_state = MODE_OFF; ///< Current FSM state
static uint64_t last_event_timestamp_ms = 0; ///< Last event timestamp

/**
 * @brief Get current time in milliseconds
 * 
 * @return Current time in ms
 */
static uint64_t get_current_time_ms() { 
    return esp_timer_get_time() / 1000; 
}

/**
 * @brief Check if timeout has occurred for current state
 * 
 * @param[in] timeout_ms Timeout duration in milliseconds
 * @return true if timeout occurred, false otherwise
 * 
 * @note Handles 64-bit timer overflow correctly
 */
static bool check_timeout(uint64_t timeout_ms) {
    uint64_t current_ms = get_current_time_ms();
    uint64_t elapsed_ms;

    if (current_ms >= last_event_timestamp_ms) {
        elapsed_ms = current_ms - last_event_timestamp_ms;
    } else {
        // Handle extremely rare 64-bit counter overflow
        elapsed_ms = (UINT64_MAX - last_event_timestamp_ms) + current_ms + 1;
    }

    return elapsed_ms > timeout_ms;
}

/**
 * @brief Send LED command to output queue
 * 
 * @param[in] cmd LED command type
 * @param[in] timestamp Command timestamp
 * @param[in] value Command parameter value
 * 
 * @note This function blocks indefinitely until the command is queued
 */
static void send_led_command(led_cmd_type_t cmd, uint32_t timestamp, int16_t value) {
    led_command_t out = {.cmd = cmd, .timestamp = timestamp, .value = value};
    xQueueSend(qOutput, &out, portMAX_DELAY);
}

/**
 * @brief Process button events
 * 
 * @param[in] button_evt Button event from integrated queue
 * @param[in] timestamp Event timestamp
 * @return true if event was processed, false otherwise
 * 
 * @note This function handles state transitions based on button events
 */
static bool process_button_event(const button_event_t *button_evt, uint32_t timestamp) {
    switch (fsm_state) {
    case MODE_OFF:
        switch (button_evt->type) {
        case BUTTON_CLICK:
        case BUTTON_LONG_CLICK:
        case BUTTON_DOUBLE_CLICK:
            fsm_state = MODE_DISPLAY;
            send_led_command(LED_CMD_TURN_ON, timestamp, 0);
            ESP_LOGI(TAG, "MODE_OFF -> MODE_DISPLAY (Button Press)");
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
            send_led_command(LED_CMD_ENTER_EFFECT_SELECT, timestamp, 0);
            send_led_command(LED_CMD_FEEDBACK_EFFECT_COLOR, timestamp, 0);
            ESP_LOGI(TAG, "MODE_DISPLAY -> MODE_EFFECT_SELECT");
            return true;
        case BUTTON_LONG_CLICK:
            fsm_state = MODE_EFFECT_SETUP;
            send_led_command(LED_CMD_ENTER_EFFECT_SETUP, timestamp, 0);
            send_led_command(LED_CMD_FEEDBACK_BLUE, timestamp, 0);
            ESP_LOGI(TAG, "MODE_DISPLAY -> MODE_EFFECT_SETUP");
            return true;
        case BUTTON_VERY_LONG_CLICK:
            fsm_state = MODE_SYSTEM_SETUP;
            led_controller_enter_system_setup();
            send_led_command(LED_CMD_FEEDBACK_BLUE, timestamp, 0);
            ESP_LOGI(TAG, "MODE_DISPLAY -> MODE_SYSTEM_SETUP");
            return true;
        default:
            return false;
        }

    case MODE_EFFECT_SELECT:
        switch (button_evt->type) {
        case BUTTON_CLICK:
            fsm_state = MODE_DISPLAY;
            uint8_t selected_effect_index = led_controller_get_effect_index();
            send_led_command(LED_CMD_SET_EFFECT, timestamp, selected_effect_index);
            send_led_command(LED_CMD_FEEDBACK_GREEN, timestamp, 0);
            ESP_LOGI(TAG, "MODE_EFFECT_SELECT -> MODE_DISPLAY (effect selected)");
            return true;
        case BUTTON_DOUBLE_CLICK:
            send_led_command(LED_CMD_CANCEL_CONFIG, timestamp, 0);
            send_led_command(LED_CMD_FEEDBACK_RED, timestamp, 0);
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
            send_led_command(LED_CMD_FEEDBACK_EFFECT_COLOR, timestamp, 0);
            ESP_LOGI(TAG, "MODE_EFFECT_SETUP Next Param");
            return true;
        case BUTTON_DOUBLE_CLICK:
            send_led_command(LED_CMD_CANCEL_CONFIG, timestamp, 0);
            send_led_command(LED_CMD_FEEDBACK_RED, timestamp, 0);
            fsm_state = MODE_DISPLAY;
            ESP_LOGI(TAG, "MODE_EFFECT_SETUP -> MODE_DISPLAY (cancelled)");
            return true;
        case BUTTON_LONG_CLICK:
            fsm_state = MODE_DISPLAY;
            send_led_command(LED_CMD_SAVE_CONFIG, timestamp, 0);
            send_led_command(LED_CMD_FEEDBACK_GREEN, timestamp, 0);
            ESP_LOGI(TAG, "MODE_EFFECT_SETUP -> MODE_DISPLAY (saved)");
            return true;
        case BUTTON_VERY_LONG_CLICK:
            fsm_state = MODE_DISPLAY;
            send_led_command(LED_CMD_SYNC_AND_SAVE_STATIC_CONFIG, timestamp, 0);
            send_led_command(LED_CMD_FEEDBACK_GREEN, timestamp, 0);
            ESP_LOGI(TAG, "MODE_EFFECT_SETUP -> MODE_DISPLAY (sync and saved)");
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
            led_controller_next_system_param();
            send_led_command(LED_CMD_FEEDBACK_BLUE, timestamp, 0);
            ESP_LOGI(TAG, "MODE_SYSTEM_SETUP Next Param");
            return true;
        case BUTTON_DOUBLE_CLICK:
            led_controller_cancel_system_config();
            send_led_command(LED_CMD_FEEDBACK_RED, timestamp, 0);
            fsm_state = MODE_DISPLAY;
            ESP_LOGI(TAG, "MODE_SYSTEM_SETUP -> MODE_DISPLAY (cancelled)");
            return true;
        case BUTTON_LONG_CLICK:
            fsm_state = MODE_DISPLAY;
            led_controller_save_system_config();
            send_led_command(LED_CMD_FEEDBACK_GREEN, timestamp, 0);
            ESP_LOGI(TAG, "MODE_SYSTEM_SETUP -> MODE_DISPLAY (saved)");
            return true;
        case BUTTON_VERY_LONG_CLICK:
            led_controller_factory_reset();
            send_led_command(LED_CMD_FEEDBACK_GREEN, timestamp, 0);
            ESP_LOGI(TAG, "MODE_SYSTEM_SETUP: Performed factory reset");
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
 * @brief Process encoder events
 * 
 * @param[in] encoder_evt Encoder event from integrated queue
 * @param[in] timestamp Event timestamp
 * @return true if event was processed, false otherwise
 */
static bool process_encoder_event(const encoder_event_t *encoder_evt, uint32_t timestamp) {
    if (encoder_evt->steps == 0) {
        return false;
    }

    int16_t steps = (int16_t)encoder_evt->steps;

    switch (fsm_state) {
    case MODE_DISPLAY: {
        bool limit_hit = false;
        uint8_t new_brightness = led_controller_inc_brightness(steps, &limit_hit);
        send_led_command(LED_CMD_SET_BRIGHTNESS, timestamp, new_brightness);
        if (limit_hit) {
            send_led_command(LED_CMD_FEEDBACK_LIMIT, timestamp, 0);
        }
        ESP_LOGD(TAG, "Brightness set to: %d", new_brightness);
        break;
    }

    case MODE_EFFECT_SELECT: {
        uint8_t new_effect_idx = led_controller_inc_effect(steps);
        send_led_command(LED_CMD_SET_EFFECT, timestamp, new_effect_idx);
        ESP_LOGD(TAG, "Effect selection preview: %d", new_effect_idx);
        break;
    }

    case MODE_EFFECT_SETUP: {
        bool limit_hit = false;
        uint16_t new_param_packed = led_controller_inc_effect_param(steps, &limit_hit);
        send_led_command(LED_CMD_SET_EFFECT_PARAM, timestamp, new_param_packed);
        if (limit_hit) {
            send_led_command(LED_CMD_FEEDBACK_LIMIT, timestamp, 0);
        }
        ESP_LOGD(TAG, "Effect param set to: %u", new_param_packed);
        break;
    }

    case MODE_SYSTEM_SETUP: {
        bool limit_hit = false;
        led_controller_inc_system_param(steps, &limit_hit);
        if (limit_hit) {
            send_led_command(LED_CMD_FEEDBACK_LIMIT, timestamp, 0);
        }
        ESP_LOGD(TAG, "System parameter adjustment: %d", steps);
        break;
    }

    case MODE_OFF:
    default:
        return false;
    }

    return true;
}

/**
 * @brief Process touch events
 * 
 * @param[in] touch_evt Touch event from integrated queue
 * @param[in] timestamp Event timestamp
 * @return true if event was processed, false otherwise
 */
static bool process_touch_event(const touch_event_t *touch_evt, uint32_t timestamp) {
    // In display mode, a simple press cycles to the next effect
    if (fsm_state == MODE_DISPLAY && touch_evt->type == TOUCH_PRESS) {
        uint8_t new_effect_idx = led_controller_inc_effect(1);
        send_led_command(LED_CMD_SET_EFFECT, timestamp, new_effect_idx);
        ESP_LOGI(TAG, "Touch press cycled to next effect: %d", new_effect_idx);
        return true;
    }

    return false;
}

/**
 * @brief Process ESPNOW events
 * 
 * @param[in] espnow_evt ESPNOW event from integrated queue
 * @param[in] timestamp Event timestamp
 * @return true if event was processed, false otherwise
 * 
 * @note Only processes events when compiled as SLAVE with ESP_NOW_ENABLED
 */
static bool process_espnow_event(const espnow_event_t *espnow_evt, uint32_t timestamp) {
#if ESP_NOW_ENABLED && IS_SLAVE
    ESP_LOGD(TAG, "Processing ESPNOW event from " MACSTR, MAC2STR(espnow_evt->mac_addr));

    // Forward received command to LED controller
    const led_command_t *cmd = &espnow_evt->msg.cmd;

    // Slave should not be in setup modes - snap back to display mode
    if (fsm_state != MODE_DISPLAY && fsm_state != MODE_OFF) {
        fsm_state = MODE_DISPLAY;
        ESP_LOGW(TAG, "Slave was in setup state, snapping back to MODE_DISPLAY");
    }

    send_led_command(cmd->cmd, cmd->timestamp, cmd->value);
    return true;
#else
    return false; // Not a slave, ignore ESP-NOW events
#endif
}

/**
 * @brief Process switch events
 * 
 * @param[in] switch_evt Switch event from integrated queue
 * @param[in] timestamp Event timestamp
 * @return true if event was processed, false otherwise
 * 
 * @note Master: controls ESP-NOW sending, Slave: controls strip mode
 */
static bool process_switch_event(const switch_event_t *switch_evt, uint32_t timestamp) {
#if IS_MASTER
    // On master, switch controls ESP-NOW sending
    bool sending_enabled = switch_evt->is_closed;
    espnow_controller_set_master_enabled(sending_enabled);

    if (sending_enabled) {
        ESP_LOGI(TAG, "Switch: ESP-NOW Master sending ENABLED. Syncing slaves...");

        // Synchronize all slaves with current state
        if (led_controller_is_on()) {
            send_led_command(LED_CMD_TURN_ON, timestamp, 0);
        } else {
            send_led_command(LED_CMD_TURN_OFF, timestamp, 0);
        }

        uint8_t effect_idx = led_controller_get_effect_index();
        send_led_command(LED_CMD_SET_EFFECT, timestamp, effect_idx);

        uint8_t brightness = led_controller_get_brightness();
        send_led_command(LED_CMD_SET_BRIGHTNESS, timestamp, brightness);

        uint8_t num_params = 0;
        effect_param_t *params = led_controller_get_effect_params(&num_params);
        if (params && num_params > 0) {
            for (uint8_t i = 0; i < num_params; i++) {
                uint16_t packed_value = (i << 8) | params[i].value;
                send_led_command(LED_CMD_SET_EFFECT_PARAM, timestamp, packed_value);
            }
        }
    }
#else
    // On slave, switch controls strip mode
    int16_t mode_value = switch_evt->is_closed ? 0 : 1;
    send_led_command(LED_CMD_SET_STRIP_MODE, timestamp, mode_value);
    ESP_LOGI(TAG, "Switch event processed, strip mode set to %d", mode_value);
#endif
    return true;
}

/**
 * @brief Main FSM task function
 * 
 * @param[in] pv Unused parameter
 * 
 * @note This task processes input events and manages state transitions
 * @warning Task priority should be appropriate for real-time response
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

            // Process event based on source
            switch (integrated_evt.source) {
            case EVENT_SOURCE_BUTTON:
                event_processed = process_button_event(&integrated_evt.data.button, integrated_evt.timestamp);
                break;

            case EVENT_SOURCE_ENCODER:
                event_processed = process_encoder_event(&integrated_evt.data.encoder, integrated_evt.timestamp);
                break;

            case EVENT_SOURCE_TOUCH:
                event_processed = process_touch_event(&integrated_evt.data.touch, integrated_evt.timestamp);
                break;

            case EVENT_SOURCE_ESPNOW:
                event_processed = process_espnow_event(&integrated_evt.data.espnow, integrated_evt.timestamp);
                break;

            case EVENT_SOURCE_SWITCH:
                event_processed = process_switch_event(&integrated_evt.data.switch_evt, integrated_evt.timestamp);
                break;

            default:
                ESP_LOGW(TAG, "Unknown event source: %d", integrated_evt.source);
                break;
            }

            if (event_processed) {
                last_event_timestamp_ms = get_current_time_ms();
            }
        } else {
            // Check for timeout in timed states
            fsm_state_t current_state = fsm_get_state();
            bool timeout = false;

            if (current_state == MODE_EFFECT_SELECT && check_timeout(TIMEOUT_EFFECT_SELECT_MS)) {
                timeout = true;
            } else if (current_state == MODE_EFFECT_SETUP && check_timeout(TIMEOUT_EFFECT_SETUP_MS)) {
                timeout = true;
            } else if (current_state == MODE_SYSTEM_SETUP && check_timeout(TIMEOUT_SYSTEM_SETUP_MS)) {
                led_controller_save_system_config();
                timeout = true;
            }

            if (timeout) {
                if (current_state != MODE_SYSTEM_SETUP) {
                    send_led_command(LED_CMD_SAVE_CONFIG, get_current_time_ms(), 0);
                }
                fsm_state = MODE_DISPLAY;
                send_led_command(LED_CMD_FEEDBACK_GREEN, get_current_time_ms(), 0);
                ESP_LOGI(TAG, "Timeout in state %d -> MODE_DISPLAY (auto-save)", current_state);
                last_event_timestamp_ms = get_current_time_ms();
            }
        }
    }
}

/**
 * @brief Initialize the FSM module
 * 
 * @param[in] inputQueue Queue for integrated input events
 * @param[in] outputQueue Queue for LED commands
 * 
 * @note This function must be called before any other FSM functions
 * @warning Queues must be properly initialized before calling
 */
void fsm_init(QueueHandle_t inputQueue, QueueHandle_t outputQueue) {
    if (!inputQueue || !outputQueue) {
        ESP_LOGE(TAG, "Invalid queue parameters");
        return;
    }

    qInput = inputQueue;
    qOutput = outputQueue;
    last_event_timestamp_ms = get_current_time_ms();

    BaseType_t result = xTaskCreate(fsm_task, "FSM", FSM_STACK_SIZE, NULL, FSM_TASK_PRIORITY, NULL);
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create FSM task");
        return;
    }

    ESP_LOGI(TAG, "FSM initialized successfully in state MODE_OFF");
}

/**
 * @brief Get current FSM state
 * 
 * @return Current FSM state
 */
fsm_state_t fsm_get_state(void) { 
    return fsm_state; 
}

/**
 * @brief Set the initial state of the FSM
 * 
 * @param[in] state The state to set as the initial state
 * 
 * @note This function resets the timeout timer for the new state
 */
void fsm_set_initial_state(fsm_state_t state) {
    fsm_state = state;
    last_event_timestamp_ms = get_current_time_ms();
    ESP_LOGI(TAG, "FSM initial state set to: %d", state);
}
#include "fsm.h"
#include "esp_log.h"

static const char *TAG = "FSM_COMPONENT";

static fsm_state_t current_fsm_state = STATE_OFF;
static QueueHandle_t s_input_queue;
static QueueHandle_t s_output_queue;

// Internal helper function to send actions to the output queue
static void send_action_to_queue(fsm_action_type_t type, void *data_payload) {
  fsm_output_action_t action;
  action.type = type;

  if (data_payload != NULL) {
    switch (type) {
    case ACTION_ADJUST_BRIGHTNESS:
      action.data.brightness_value = *((uint8_t *)data_payload);
      break;
    case ACTION_SELECT_EFFECT:
      action.data.effect_id = *((uint8_t *)data_payload);
      break;
    case ACTION_SAVE_EFFECT_PARAMETER:
      action.data.effect_param = *((effect_param_t *)data_payload);
      break;
    default:
      break;
    }
  }

  if (xQueueSend(s_output_queue, &action, portMAX_DELAY) != pdPASS) {
    ESP_LOGE(TAG, "Failed to send action to output queue");
  }
}

void fsm_init(QueueHandle_t input_queue, QueueHandle_t output_queue) {
  s_input_queue = input_queue;
  s_output_queue = output_queue;
  ESP_LOGI(TAG, "FSM initialized with input and output queues");
}

void fsm_task(void *pvParameters) {
  integrated_event_t received_event;

  ESP_LOGI(TAG, "FSM task started");

  for (;;) {
    if (xQueueReceive(s_input_queue, &received_event, portMAX_DELAY) ==
        pdPASS) {
      ESP_LOGI(TAG, "Event received: Source = %d, Timestamp = %" PRIu32 "",
               received_event.source, received_event.timestamp);

      switch (current_fsm_state) {
      case STATE_OFF:
        if (received_event.source == EVENT_SOURCE_BUTTON &&
            received_event.data.button.type == BUTTON_CLICK) {
          ESP_LOGI(TAG, "Transitioning from OFF to DISPLAY");
          send_action_to_queue(ACTION_TURN_ON_LAMP, NULL);
          // In a real scenario, load last effect/brightness here
          current_fsm_state = STATE_DISPLAY;
        }
        break;

      case STATE_DISPLAY:
        if (received_event.source == EVENT_SOURCE_ENCODER) {
          ESP_LOGI(TAG, "Adjusting brightness in DISPLAY mode");
          // Example: adjust brightness based on encoder steps
          uint8_t new_brightness =
              128; // Placeholder for actual brightness calculation
          send_action_to_queue(ACTION_ADJUST_BRIGHTNESS, &new_brightness);
        } else if (received_event.source == EVENT_SOURCE_BUTTON) {
          if (received_event.data.button.type == BUTTON_CLICK) {
            ESP_LOGI(TAG, "Transitioning from DISPLAY to OFF");
            send_action_to_queue(ACTION_TURN_OFF_LAMP, NULL);
            current_fsm_state = STATE_OFF;
          } else if (received_event.data.button.type == BUTTON_LONG_CLICK) {
            ESP_LOGI(TAG, "Transitioning from DISPLAY to EFFECT_SETUP");
            current_fsm_state = STATE_EFFECT_SETUP;
          }
        }
        break;

      case STATE_EFFECT_CHANGE:
        if (received_event.source == EVENT_SOURCE_ENCODER) {
          ESP_LOGI(TAG, "Selecting effect in EFFECT_CHANGE mode");
          // Logic to select next/previous effect based on encoder steps
          uint8_t selected_effect_id = 1; // Placeholder
          send_action_to_queue(ACTION_SELECT_EFFECT, &selected_effect_id);
        } else if (received_event.source == EVENT_SOURCE_BUTTON) {
          if (received_event.data.button.type == BUTTON_CLICK) {
            ESP_LOGI(TAG, "Confirming effect and transitioning from "
                          "EFFECT_CHANGE to DISPLAY");
            send_action_to_queue(ACTION_CONFIRM_EFFECT, NULL);
            current_fsm_state = STATE_DISPLAY;
          } else if (received_event.data.button.type == BUTTON_DOUBLE_CLICK) {
            ESP_LOGI(TAG, "Reverting effect and transitioning from "
                          "EFFECT_CHANGE to DISPLAY");
            send_action_to_queue(ACTION_REVERT_EFFECT_PARAMETERS,
                                 NULL); // Revert to previous effect
            current_fsm_state = STATE_DISPLAY;
          }
        } else if (received_event.source == EVENT_SOURCE_BUTTON &&
                   received_event.data.button.type == BUTTON_TIMEOUT) {
          ESP_LOGI(TAG, "Timeout in EFFECT_CHANGE, confirming current effect "
                        "and transitioning to DISPLAY");
          send_action_to_queue(ACTION_CONFIRM_EFFECT, NULL);
          current_fsm_state = STATE_DISPLAY;
        }
        break;

      case STATE_EFFECT_SETUP:
        if (received_event.source == EVENT_SOURCE_ENCODER) {
          ESP_LOGI(TAG, "Adjusting effect parameter in EFFECT_SETUP mode");
          // Logic to adjust current parameter value
          struct {
            uint8_t param_id;
            int32_t param_value;
          } effect_param_data = {.param_id = 0,
                                 .param_value = 0}; // Placeholder
          send_action_to_queue(ACTION_SAVE_EFFECT_PARAMETER,
                               &effect_param_data);
        } else if (received_event.source == EVENT_SOURCE_BUTTON) {
          if (received_event.data.button.type == BUTTON_CLICK) {
            ESP_LOGI(TAG,
                     "Saving parameter and advancing in EFFECT_SETUP mode");
            // Logic to save current parameter and move to next, or exit if all
            // done If all parameters are set, transition to DISPLAY
            // current_fsm_state = STATE_DISPLAY;
          } else if (received_event.data.button.type == BUTTON_LONG_CLICK) {
            ESP_LOGI(TAG, "Exiting EFFECT_SETUP to DISPLAY");
            current_fsm_state = STATE_DISPLAY;
          } else if (received_event.data.button.type == BUTTON_DOUBLE_CLICK) {
            ESP_LOGI(TAG, "Cancelling EFFECT_SETUP and returning to DISPLAY");
            send_action_to_queue(ACTION_REVERT_EFFECT_PARAMETERS, NULL);
            current_fsm_state = STATE_DISPLAY;
          } else if (received_event.data.button.type ==
                     BUTTON_VERY_LONG_CLICK) {
            ESP_LOGI(TAG, "Transitioning from EFFECT_SETUP to SYSTEM_SETUP");
            current_fsm_state = STATE_SYSTEM_SETUP;
          }
        }
        break;

      case STATE_SYSTEM_SETUP:
        // Logic for System Setup Mode
        // This mode would handle general system parameters.
        // Transitions would typically lead back to DISPLAY mode.
        ESP_LOGI(TAG, "Currently in SYSTEM_SETUP mode");
        // Example: if a button click, exit system setup
        if (received_event.source == EVENT_SOURCE_BUTTON &&
            received_event.data.button.type == BUTTON_CLICK) {
          ESP_LOGI(TAG, "Exiting SYSTEM_SETUP to DISPLAY");
          current_fsm_state = STATE_DISPLAY;
        }
        break;

      default:
        ESP_LOGW(TAG, "Unknown FSM state: %d", current_fsm_state);
        break;
      }
    }
  }
}

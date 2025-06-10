#include "fsm.h"
#include "esp_timer.h"
#include "led_controller.h" // Para controle dos LEDs
#include "project_config.h" // For LED_STRIP_NUM_LEDS etc.
#include <inttypes.h>       // For PRId32 macro
#include <stdlib.h>         // For malloc, free
#include <string.h>         // For memset, memcpy

#include "driver/spi_common.h" // For SPI_CLK_SRC_DEFAULT
#include "led_effects.h"       // For led_effect_t, effect_param_t
#include "led_strip_types.h"   // For LED_PIXEL_FORMAT_GRB, LED_MODEL_SK6812

static const char *TAG = "FSM";

// LED Effects state (global to FSM component)
static const led_effect_t *current_active_effect = NULL;
static effect_param_t *current_effect_params_runtime =
    NULL; // Mutable copy for runtime adjustments
static uint8_t num_defined_effects = 0;

// System Parameters
typedef struct {
  char name[30];
  int32_t value;
  int32_t min;
  int32_t max;
  int32_t step;
} system_param_t;

#define MAX_SYSTEM_PARAMS 2
static system_param_t system_params_array[MAX_SYSTEM_PARAMS] = {
    {"Mode Timeout (s)", 30, 5, 120, 5}, // Stored in seconds
    {"Def Brightness", 128, 10, 254, 10} // Default brightness
};
static const uint8_t num_system_params = MAX_SYSTEM_PARAMS;
static system_param_t
    system_params_temp_copy[MAX_SYSTEM_PARAMS]; // For "cancel" functionality

// Estrutura de contexto da FSM
typedef struct {
  // Configuração
  QueueHandle_t event_queue;    ///< Handle da queue de eventos
  TaskHandle_t fsm_task_handle; ///< Handle da task da FSM
  fsm_mode_t config;            ///< Configurações

  // Estado atual
  fsm_state_t current_state;  ///< Estado atual da FSM
  fsm_state_t previous_state; ///< Estado anterior (para rollback)
  uint32_t state_entry_time;  ///< Timestamp de entrada no estado atual

  // Dados do sistema
  uint8_t current_effect;     ///< ID do efeito atualmente selecionado
  uint8_t previous_effect_id; ///< ID do efeito anterior (para rollback em
                              ///< MODE_EFFECT_SELECT)
  uint8_t global_brightness;  ///< Brilho global (0-254)
  bool led_strip_on;          ///< Estado da fita LED

  // Setup state
  uint8_t setup_param_index;  ///< Parâmetro sendo editado no setup do efeito
  uint8_t setup_effect_index; ///< ID do efeito sendo editado no setup
  bool setup_has_changes; ///< Flag de mudanças não salvas no setup do efeito

  // System Setup state
  uint8_t system_setup_param_index; ///< Parâmetro do sistema sendo editado
  bool system_setup_has_changes; ///< Flag de mudanças não salvas no setup do
                                 ///< sistema

  // Controle
  bool initialized; ///< Flag de inicialização
  bool running;     ///< Flag de execução

  // Estatísticas
  fsm_stats_t stats; ///< Estatísticas da FSM
} fsm_context_t;

static fsm_context_t fsm_ctx = {0};

// Declarações das funções internas
static void fsm_task(void *pvParameters);
static esp_err_t fsm_process_integrated_event(const integrated_event_t *event);
static esp_err_t fsm_process_button_event(const button_event_t *button_event,
                                          uint32_t timestamp);
static esp_err_t fsm_process_encoder_event(const encoder_event_t *encoder_event,
                                           uint32_t timestamp);
static esp_err_t fsm_process_espnow_event(const espnow_event_t *espnow_event,
                                          uint32_t timestamp);
static esp_err_t fsm_transition_to_state(fsm_state_t new_state);
static void fsm_check_mode_timeout(uint32_t current_time);
static void fsm_update_led_display(void);
static uint32_t fsm_get_current_time_ms(void);
static void fsm_load_effect_params(uint8_t effect_id);

// Visual Feedback
typedef enum {
  FEEDBACK_ENTERING_DISPLAY_MODE, // Optional: if useful
  FEEDBACK_ENTERING_EFFECT_SELECT,
  FEEDBACK_ENTERING_EFFECT_SETUP,
  FEEDBACK_ENTERING_SYSTEM_SETUP,
  FEEDBACK_SAVED_PARAM, // When a single parameter is confirmed by click in
                        // setup
  FEEDBACK_SAVED_EFFECT_SETTINGS, // When exiting setup with changes via long
                                  // click or last param click
  FEEDBACK_CANCEL_SETUP,    // When exiting setup via double click (changes
                            // discarded)
  FEEDBACK_EFFECT_SELECTED, // When an effect is chosen in select mode by click
  FEEDBACK_REVERT_EFFECT_SELECTION, // When selection is cancelled by double
                                    // click
  FEEDBACK_POWER_ON,
  FEEDBACK_POWER_OFF,
  FEEDBACK_SYSTEM_PARAM_SELECTED,  // Click in system setup to go to next param
  FEEDBACK_SYSTEM_SETTINGS_SAVED,  // System settings applied
  FEEDBACK_SYSTEM_SETUP_CANCELLED, // Exited system setup, changes for this
                                   // session discarded
  FEEDBACK_EXIT_SYSTEM_SETUP, // Exited system setup (changes might have been
                              // saved)
} fsm_feedback_event_t;

static void fsm_perform_visual_feedback(fsm_feedback_event_t feedback_type);
static void fsm_apply_system_settings(
    void); // Forward declaration for fsm_apply_system_settings

esp_err_t fsm_init(QueueHandle_t queue_handle, const fsm_mode_t *config) {
  if (fsm_ctx.initialized) {
    ESP_LOGW(TAG, "FSM already initialized");
    return ESP_ERR_INVALID_STATE;
  }

  if (queue_handle == NULL) {
    ESP_LOGE(TAG, "Queue handle cannot be NULL");
    return ESP_ERR_INVALID_ARG;
  }

  // Configura parâmetros (usa defaults se config for NULL)
  if (config != NULL) {
    fsm_ctx.config = *config;
  } else {
    fsm_ctx.config.task_stack_size = FSM_STACK_SIZE;
    fsm_ctx.config.task_priority = FSM_PRIORITY;
    fsm_ctx.config.queue_timeout_ms = pdMS_TO_TICKS(FSM_TIMEOUT_MS);
    fsm_ctx.config.mode_timeout_ms = FSM_MODE_TIMEOUT_MS;
  }

  fsm_ctx.event_queue = queue_handle;

  // Inicializa estado inicial
  fsm_ctx.current_state = MODE_DISPLAY;
  fsm_ctx.previous_state = MODE_DISPLAY;
  fsm_ctx.state_entry_time = fsm_get_current_time_ms();

  // Inicializa dados do sistema com valores padrão
  fsm_ctx.current_effect = 0;      // Default to first effect ID
  fsm_ctx.previous_effect_id = 0;  // Initialize to default effect ID
  fsm_ctx.global_brightness = 128; // 50% de brilho inicial
  fsm_ctx.led_strip_on = true;
  fsm_ctx.setup_param_index = 0;
  fsm_ctx.setup_effect_index =
      0; // Will be set to current_effect when entering setup mode
  fsm_ctx.setup_has_changes = false;

  fsm_ctx.system_setup_param_index = 0;
  fsm_ctx.system_setup_has_changes = false;

  // TODO: Load system_params_array from NVS here in a real implementation
  // For now, apply default values from the static array for
  // config.mode_timeout_ms This ensures that if fsm_ctx.config was passed in,
  // its mode_timeout_ms is respected unless overwritten by system settings
  // later or if config was NULL.
  if (config ==
      NULL) { // Only apply from system_params_array if no config was supplied
    fsm_ctx.config.mode_timeout_ms = system_params_array[0].value * 1000;
  }
  // Note: Default brightness from system_params_array[1] is not automatically
  // applied to fsm_ctx.global_brightness here. It's a setting that can be
  // manually applied via system setup, or could be used at initial cold boot.

  // Limpa estatísticas
  memset(&fsm_ctx.stats, 0, sizeof(fsm_stats_t));
  fsm_ctx.stats.current_state = fsm_ctx.current_state;

  typedef struct {
    spi_host_device_t
        spi_host; ///< SPI host device (e.g., SPI2_HOST, SPI3_HOST). The SPI bus
                  ///< should be initialized before calling
                  ///< `led_controller_init`.
    uint32_t clk_speed_hz; ///< SPI clock speed in Hertz. Common values are
                           ///< 10MHz (10 * 1000 * 1000).
    uint32_t num_leds; ///< Number of LEDs in the strip. This defines the size
                       ///< of the buffer.
    uint8_t spi_mosi_gpio; ///< GPIO number used for the SPI MOSI (Data) signal.
    uint8_t
        spi_sclk_gpio; ///< GPIO number used for the SPI SCLK (Clock) signal.
    led_color_component_format_t
        pixel_format;  ///< Pixel format of the LED strip (e.g.,
                       ///< LED_PIXEL_FORMAT_GRB).
    led_model_t model; ///< Model of the LED strip (e.g., LED_MODEL_SK6812,
                       ///< LED_MODEL_WS2812).
    spi_clock_source_t
        spi_clk_src; ///< SPI clock source. Use from `spi_common.h` (e.g.,
                     ///< SPI_CLK_SRC_DEFAULT).
  } led_controller_config_t;

  // Initialize LED controller
  const led_controller_config_t led_cfg = {
      .spi_host = LED_STRIP_SPI_HOST,             // From project_config.h
      .clk_speed_hz = LED_STRIP_SPI_CLK_SPEED_HZ, // From project_config.h
      .num_leds = LED_STRIP_NUM_LEDS,             // From project_config.h
      .spi_mosi_gpio = LED_STRIP_GPIO_MOSI,       // From project_config.h
      .spi_sclk_gpio = LED_STRIP_GPIO_SCLK,       // From project_config.h
      .pixel_format =
          {
              .format =
                  {
                      .r_pos = 1, // red is the second byte in the color data
                      .g_pos = 0, // green is the first byte in the color data
                      .b_pos = 2, // blue is the third byte in the color data
                      .num_components = 3, // total 3 color components
                  },
          },
      .model = LED_MODEL_WS2812,         // Using direct value
      .spi_clk_src = SPI_CLK_SRC_DEFAULT // Using direct value
  };
  
  esp_err_t ret_led = led_controller_init(&led_cfg);
  if (ret_led != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize LED controller: %s",
             esp_err_to_name(ret_led));
    // Handle error, perhaps prevent FSM from starting fully
    // For now, we'll continue, but LEDs won't work.
  } else {
    ESP_LOGI(TAG, "LED controller initialized.");
    // Load available effects
    const led_effect_t *effects_array =
        led_effects_get_all(&num_defined_effects);
    if (num_defined_effects > 0 && effects_array != NULL) {
      ESP_LOGI(TAG,
               "%" PRIu8 " LED effects defined. Defaulting to effect ID %" PRIu8
               ".",
               num_defined_effects, fsm_ctx.current_effect);
      fsm_load_effect_params(
          fsm_ctx.current_effect); // Load params for the default effect
      if (current_active_effect == NULL) {
        ESP_LOGE(TAG, "Failed to load default effect %" PRIu8 ".",
                 fsm_ctx.current_effect);
        // Potentially try to load the absolute first effect if default fails
        if (fsm_ctx.current_effect != effects_array[0].id) {
          fsm_ctx.current_effect = effects_array[0].id;
          fsm_load_effect_params(fsm_ctx.current_effect);
        }
        if (current_active_effect == NULL &&
            num_defined_effects > 0) { // Still failed
          ESP_LOGE(TAG, "Failed to load even the first defined effect. LED "
                        "effects will be unavailable.");
        }
      }
    } else {
      ESP_LOGW(TAG, "No LED effects defined!");
      num_defined_effects = 0; // Ensure it's zero
    }
    // Apply initial brightness
    led_controller_set_brightness(fsm_ctx.global_brightness);
  }

  // Cria a task da FSM
  BaseType_t result =
      xTaskCreate(fsm_task, "fsm_task", fsm_ctx.config.task_stack_size, NULL,
                  fsm_ctx.config.task_priority, &fsm_ctx.fsm_task_handle);

  if (result != pdPASS) {
    ESP_LOGE(TAG, "Failed to create FSM task");
    return ESP_ERR_NO_MEM;
  }

  fsm_ctx.initialized = true;
  fsm_ctx.running = true;

  // Inicializa display dos LEDs (after LED controller and effects are loaded)
  if (ret_led == ESP_OK) {
    fsm_update_led_display();
  }

  ESP_LOGI(TAG, "FSM initialized successfully in state %d",
           (int)fsm_ctx.current_state); // Enum usually fine with %d
  return ESP_OK;
}

esp_err_t fsm_deinit(void) {
  if (!fsm_ctx.initialized) {
    return ESP_ERR_INVALID_STATE;
  }

  fsm_ctx.running = false;

  // Aguarda a task terminar
  if (fsm_ctx.fsm_task_handle != NULL) {
    vTaskDelete(fsm_ctx.fsm_task_handle);
    fsm_ctx.fsm_task_handle = NULL;
  }

  // Deinitialize LED controller
  esp_err_t deinit_ret = led_controller_deinit();
  if (deinit_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to deinitialize LED controller: %s",
             esp_err_to_name(deinit_ret));
  } else {
    ESP_LOGI(TAG, "LED controller deinitialized.");
  }

  // Free runtime effect parameters
  if (current_effect_params_runtime != NULL) {
    free(current_effect_params_runtime);
    current_effect_params_runtime = NULL;
    current_active_effect = NULL; // Also clear the pointer to the const effect
    num_defined_effects = 0;
  }

  fsm_ctx.initialized = false;
  ESP_LOGI(TAG, "FSM deinitialized");
  return ESP_OK;
}

bool fsm_is_running(void) { return fsm_ctx.initialized && fsm_ctx.running; }

fsm_state_t fsm_get_current_state(void) { return fsm_ctx.current_state; }

uint8_t fsm_get_current_effect(void) { return fsm_ctx.current_effect; }

uint8_t fsm_get_global_brightness(void) { return fsm_ctx.global_brightness; }

bool fsm_is_led_strip_on(void) { return fsm_ctx.led_strip_on; }

esp_err_t fsm_get_stats(fsm_stats_t *stats) {
  if (stats == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  fsm_ctx.stats.current_state =
      fsm_ctx.current_state; // This is fsm_state_t (enum)
  fsm_ctx.stats.time_in_current_state =
      fsm_get_current_time_ms() - fsm_ctx.state_entry_time; // uint32_t
  *stats = fsm_ctx.stats;
  return ESP_OK;
}

void fsm_reset_stats(void) {
  memset(&fsm_ctx.stats, 0, sizeof(fsm_stats_t));
  fsm_ctx.stats.current_state = fsm_ctx.current_state;
  ESP_LOGI(TAG, "Statistics reset");
}

esp_err_t fsm_force_state(fsm_state_t new_state) {
  if (new_state >=
      MODE_SYSTEM_SETUP +
          1) { // Assuming MODE_SYSTEM_SETUP is the last valid state
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGW(TAG, "Forcing state transition from %d to %d",
           (int)fsm_ctx.current_state, (int)new_state); // Enums with %d
  return fsm_transition_to_state(new_state);
}

// Task principal da FSM
static void fsm_task(void *pvParameters) {
  integrated_event_t event;
  uint32_t current_time; // uint32_t

  ESP_LOGI(TAG, "FSM task started");

  while (fsm_ctx.running) {
    // Espera por eventos na queue
    if (xQueueReceive(fsm_ctx.event_queue, &event,
                      fsm_ctx.config.queue_timeout_ms) ==
        pdTRUE) { // queue_timeout_ms is TickType_t (uint32_t)
      fsm_ctx.stats.events_processed++; // uint32_t

      esp_err_t ret = fsm_process_integrated_event(&event);
      if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Error processing event: %s", esp_err_to_name(ret));
      }
    } else {
      // Timeout na queue - verifica timeouts do sistema
      fsm_ctx.stats.queue_timeouts++; // uint32_t
    }

    // Verifica timeout de modo (volta para DISPLAY se ficar muito tempo em
    // outros modos)
    current_time = fsm_get_current_time_ms(); // uint32_t
    fsm_check_mode_timeout(current_time);
  }

  ESP_LOGI(TAG, "FSM task terminated");
  vTaskDelete(NULL);
}

// Processa evento integrado
static esp_err_t fsm_process_integrated_event(const integrated_event_t *event) {
  if (event == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  esp_err_t ret = ESP_OK;

  // Processa evento baseado na fonte
  switch (event->source) {
  case EVENT_SOURCE_BUTTON:
    // event.timestamp is uint32_t
    ret = fsm_process_button_event(&event->data.button, event->timestamp);
    fsm_ctx.stats.button_events++; // uint32_t
    break;

  case EVENT_SOURCE_ENCODER:
    // event.timestamp is uint32_t
    ret = fsm_process_encoder_event(&event->data.encoder, event->timestamp);
    fsm_ctx.stats.encoder_events++; // uint32_t
    break;

  case EVENT_SOURCE_ESPNOW:
    // event.timestamp is uint32_t
    ret = fsm_process_espnow_event(&event->data.espnow, event->timestamp);
    fsm_ctx.stats.espnow_events++; // uint32_t
    break;

  default:
    ESP_LOGW(TAG, "Unknown event source: %d",
             (int)event->source);   // enum with %d
    fsm_ctx.stats.invalid_events++; // uint32_t
    ret = ESP_ERR_INVALID_ARG;
    break;
  }

  return ret;
}

// Processa eventos de botão baseado no estado atual
static esp_err_t fsm_process_button_event(const button_event_t *button_event,
                                          uint32_t timestamp) {
  if (button_event == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGD(TAG, "Button event: type %d in state %d", (int)button_event->type,
           (int)fsm_ctx.current_state); // enums with %d

  switch (fsm_ctx.current_state) {
  case MODE_DISPLAY:
    switch (button_event->type) {
    case BUTTON_CLICK:
      fsm_ctx.led_strip_on = !fsm_ctx.led_strip_on;
      fsm_perform_visual_feedback(fsm_ctx.led_strip_on ? FEEDBACK_POWER_ON
                                                       : FEEDBACK_POWER_OFF);
      fsm_update_led_display();
      ESP_LOGI(TAG, "LED strip %s", fsm_ctx.led_strip_on ? "ON" : "OFF");
      break;
    case BUTTON_DOUBLE_CLICK:
      if (num_defined_effects > 0) {
        fsm_ctx.previous_effect_id = fsm_ctx.current_effect;
        fsm_transition_to_state(MODE_EFFECT_SELECT);
      } else {
        ESP_LOGW(TAG, "No effects defined, cannot enter EFFECT_SELECT mode.");
      }
      break;
    case BUTTON_LONG_CLICK:
      if (current_active_effect != NULL &&
          current_active_effect->param_count > 0) {
        fsm_ctx.setup_effect_index = fsm_ctx.current_effect;
        if (current_active_effect->id != fsm_ctx.current_effect) {
          ESP_LOGI(TAG, "Loading params for effect %" PRIu8 " before setup.",
                   fsm_ctx.current_effect);
          fsm_load_effect_params(fsm_ctx.current_effect);
        }
        if (current_active_effect &&
            current_active_effect->id == fsm_ctx.setup_effect_index) {
          fsm_ctx.setup_param_index = 0;
          fsm_ctx.setup_has_changes = false;
          fsm_transition_to_state(MODE_EFFECT_SETUP);
        } else {
          ESP_LOGE(TAG,
                   "Failed to load effect %" PRIu8
                   " for setup. Staying in Display mode.",
                   fsm_ctx.current_effect);
        }
      } else {
        ESP_LOGW(TAG,
                 "Current effect '%s' has no parameters or no effect active; "
                 "cannot enter EFFECT_SETUP.",
                 current_active_effect ? current_active_effect->name : "None");
      }
      break;

    case BUTTON_VERY_LONG_CLICK:
      // Entra no setup do sistema
      fsm_transition_to_state(MODE_SYSTEM_SETUP);
      break;

    default:
      // Outros tipos de clique são ignorados neste estado
      break;
    }
    break;

  case MODE_EFFECT_SELECT:
    switch (button_event->type) {
    case BUTTON_CLICK:
      // Seleciona o efeito atualmente previewed (fsm_ctx.current_effect) e
      // volta para exibição
      fsm_perform_visual_feedback(FEEDBACK_EFFECT_SELECTED);
      fsm_transition_to_state(MODE_DISPLAY);
      ESP_LOGI(TAG, "Effect %s (ID: %" PRIu8 ") selected.",
               current_active_effect ? current_active_effect->name : "None",
               fsm_ctx.current_effect);
      break;

    case BUTTON_DOUBLE_CLICK:
      ESP_LOGI(TAG,
               "Effect selection: Double click - reverting to effect ID %" PRIu8
               ".",
               fsm_ctx.previous_effect_id);
      fsm_ctx.current_effect = fsm_ctx.previous_effect_id;
      fsm_load_effect_params(fsm_ctx.current_effect);
      fsm_perform_visual_feedback(FEEDBACK_REVERT_EFFECT_SELECTION);
      fsm_transition_to_state(MODE_DISPLAY);
      break;

    case BUTTON_LONG_CLICK:
      ESP_LOGI(TAG,
               "Exiting effect selection (Long Click). Reverting to effect ID "
               "%" PRIu8 ".",
               fsm_ctx.previous_effect_id);
      fsm_ctx.current_effect = fsm_ctx.previous_effect_id;
      fsm_load_effect_params(fsm_ctx.current_effect);
      fsm_transition_to_state(MODE_DISPLAY);
      break;

    default:
      break;
    }
    break;

  case MODE_EFFECT_SETUP:
    switch (button_event->type) {
    case BUTTON_CLICK:
      // Advance to next parameter or save and exit if last parameter
      if (current_active_effect != NULL &&
          current_effect_params_runtime != NULL &&
          current_active_effect->param_count > 0) {
        fsm_perform_visual_feedback(
            FEEDBACK_SAVED_PARAM); // Feedback for saving current param
        fsm_ctx.setup_param_index++;
        if (fsm_ctx.setup_param_index >= current_active_effect->param_count) {
          ESP_LOGI(TAG, "All parameters for '%s' configured.",
                   current_active_effect->name);
          if (fsm_ctx.setup_has_changes) {
            ESP_LOGI(TAG, "Changes to '%s' applied (runtime).",
                     current_active_effect->name);
            // NVS Save would go here
            fsm_ctx.setup_has_changes = false;
          }
          fsm_perform_visual_feedback(FEEDBACK_SAVED_EFFECT_SETTINGS);
          fsm_transition_to_state(MODE_DISPLAY);
        } else {
          ESP_LOGI(
              TAG, "Moving to edit param %" PRIu8 " ('%s') for effect '%s'",
              fsm_ctx.setup_param_index,
              current_effect_params_runtime[fsm_ctx.setup_param_index].name,
              current_active_effect->name);
        }
      } else {
        ESP_LOGW(TAG, "No effect/params in setup, returning to display.");
        fsm_transition_to_state(MODE_DISPLAY);
      }
      break;

    case BUTTON_DOUBLE_CLICK:
      ESP_LOGI(TAG,
               "Effect setup: Double click - cancelling changes for effect ID "
               "%" PRIu8 ".",
               fsm_ctx.setup_effect_index);
      fsm_ctx.current_effect = fsm_ctx.setup_effect_index;
      fsm_load_effect_params(fsm_ctx.current_effect);
      fsm_perform_visual_feedback(FEEDBACK_CANCEL_SETUP);
      fsm_transition_to_state(MODE_DISPLAY);
      break;

    case BUTTON_LONG_CLICK:
      ESP_LOGI(TAG, "Exiting effect setup for '%s'.",
               current_active_effect ? current_active_effect->name : "N/A");
      if (fsm_ctx.setup_has_changes) {
        ESP_LOGI(TAG, "Runtime changes to '%s' are kept (NVS TODO).",
                 current_active_effect ? current_active_effect->name : "N/A");
        // NVS Save would go here
        fsm_perform_visual_feedback(FEEDBACK_SAVED_EFFECT_SETTINGS);
        fsm_ctx.setup_has_changes = false;
      } else {
        // fsm_perform_visual_feedback(FEEDBACK_EXIT_SETUP_NO_CHANGES); //
        // Optional distinct feedback
      }
      fsm_ctx.current_effect = fsm_ctx.setup_effect_index;
      fsm_transition_to_state(MODE_DISPLAY);
      break;

    default:
      break;
    }
    break;

  case MODE_SYSTEM_SETUP:
    switch (button_event->type) {
    case BUTTON_CLICK: // Next parameter / Save if last
      fsm_perform_visual_feedback(FEEDBACK_SYSTEM_PARAM_SELECTED);
      fsm_ctx.system_setup_param_index++;
      if (fsm_ctx.system_setup_param_index >= num_system_params) {
        ESP_LOGI(TAG, "All system parameters configured.");
        if (fsm_ctx.system_setup_has_changes) {
          memcpy(system_params_array, system_params_temp_copy,
                 sizeof(system_params_array));
          fsm_apply_system_settings(); // This also resets
                                       // system_setup_has_changes
          fsm_perform_visual_feedback(FEEDBACK_SYSTEM_SETTINGS_SAVED);
        } else {
          // No specific feedback if no changes, just exiting.
        }
        fsm_transition_to_state(MODE_DISPLAY);
      } else {
        ESP_LOGI(
            TAG, "System Setup: Now editing: %s (Value: %" PRId32 ")",
            system_params_temp_copy[fsm_ctx.system_setup_param_index].name,
            system_params_temp_copy[fsm_ctx.system_setup_param_index].value);
      }
      break;

    case BUTTON_DOUBLE_CLICK: // Cancel without saving current session's changes
      ESP_LOGI(TAG, "System setup cancelled via double click. Changes in this "
                    "session discarded.");
      fsm_perform_visual_feedback(FEEDBACK_SYSTEM_SETUP_CANCELLED);
      // system_params_array remains untouched by this session's changes because
      // we only modified system_params_temp_copy. No need to copy back from a
      // non-existent pristine backup for this session.
      fsm_transition_to_state(MODE_DISPLAY);
      break;

    case BUTTON_LONG_CLICK: // Save and Exit
      ESP_LOGI(TAG, "Exiting System Setup (Long Click).");
      if (fsm_ctx.system_setup_has_changes) {
        ESP_LOGI(TAG, "Applying changes from system setup.");
        memcpy(system_params_array, system_params_temp_copy,
               sizeof(system_params_array));
        fsm_apply_system_settings(); // This also resets
                                     // system_setup_has_changes
        fsm_perform_visual_feedback(FEEDBACK_SYSTEM_SETTINGS_SAVED);
      } else {
        fsm_perform_visual_feedback(
            FEEDBACK_EXIT_SYSTEM_SETUP); // Exiting without changes to save
      }
      fsm_transition_to_state(MODE_DISPLAY);
      break;

    default:
      break;
    }
    break;

  default:
    ESP_LOGW(TAG, "Unknown state in button processing: %d",
             (int)fsm_ctx.current_state); // enum
    fsm_ctx.stats.invalid_events++;       // uint32_t
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

// Processa eventos de encoder baseado no estado atual
static esp_err_t fsm_process_encoder_event(const encoder_event_t *encoder_event,
                                           uint32_t timestamp) {
  if (encoder_event == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  ESP_LOGD(TAG, "Encoder event: %" PRId32 " steps in state %d",
           encoder_event->steps,
           (int)fsm_ctx.current_state); // steps is int32_t

  switch (fsm_ctx.current_state) {
  case MODE_DISPLAY:
    int32_t new_brightness = (int32_t)fsm_ctx.global_brightness +
                             encoder_event->steps; // steps is int32_t
    if (new_brightness < 0)
      new_brightness = 0;
    if (new_brightness > 255)
      new_brightness = 255;
    fsm_ctx.global_brightness = (uint8_t)new_brightness;

    led_controller_set_brightness(fsm_ctx.global_brightness);
    fsm_update_led_display();
    ESP_LOGD(TAG, "Brightness adjusted to %" PRIu8,
             fsm_ctx.global_brightness); // global_brightness is uint8_t
    break;

  case MODE_EFFECT_SELECT:
    if (num_defined_effects == 0)
      break;

    int32_t new_effect_idx_signed =
        fsm_ctx.current_effect +
        encoder_event->steps; // current_effect is uint8_t, steps int32_t
    uint8_t new_effect_id;

    if (new_effect_idx_signed < 0) {
      new_effect_id = (num_defined_effects > 0)
                          ? (num_defined_effects - 1)
                          : 0; // num_defined_effects is uint8_t
    } else if (new_effect_idx_signed >= num_defined_effects) {
      new_effect_id = 0;
    } else {
      new_effect_id = (uint8_t)new_effect_idx_signed;
    }

    const led_effect_t *temp_effect = led_effects_get_by_id(new_effect_id);
    if (temp_effect == NULL && num_defined_effects > 0) {
      ESP_LOGW(TAG,
               "Effect ID %" PRIu8 " from index %" PRId32
               " not found, trying to find next valid.",
               new_effect_id, new_effect_idx_signed);
      const led_effect_t *all_fx = led_effects_get_all(NULL);
      int current_known_idx = -1;
      for (uint8_t i = 0; i < num_defined_effects; ++i) { // i is uint8_t
        if (all_fx[i].id == fsm_ctx.current_effect) {     // id is uint8_t
          current_known_idx = i;
          break;
        }
      }
      if (current_known_idx != -1) {
        int next_idx =
            (current_known_idx + encoder_event->steps); // steps is int32_t
        while (next_idx < 0)
          next_idx += num_defined_effects;
        next_idx %= num_defined_effects;
        new_effect_id = all_fx[next_idx].id; // id is uint8_t
      } else {
        new_effect_id = (num_defined_effects > 0) ? all_fx[0].id : 0;
      }
    }

    if (new_effect_id != fsm_ctx.current_effect) {
      fsm_ctx.current_effect = new_effect_id;
      fsm_load_effect_params(fsm_ctx.current_effect);
      fsm_update_led_display();
    }
    ESP_LOGD(TAG, "Effect selection preview: %s (ID: %" PRIu8 ")",
             current_active_effect ? current_active_effect->name : "None",
             fsm_ctx.current_effect);
    break;

  case MODE_EFFECT_SETUP:
    if (current_active_effect == NULL ||
        current_effect_params_runtime == NULL ||
        current_active_effect->param_count == 0) {
      ESP_LOGW(
          TAG,
          "In MODE_EFFECT_SETUP but no active effect or parameters to adjust.");
      break;
    }
    if (fsm_ctx.setup_param_index >=
        current_active_effect
            ->param_count) { // setup_param_index, param_count are uint8_t
      ESP_LOGE(TAG,
               "setup_param_index %" PRIu8
               " out of bounds for effect %s (count %" PRIu8 ")",
               fsm_ctx.setup_param_index, current_active_effect->name,
               current_active_effect->param_count);
      break;
    }

    effect_param_t *param_to_edit =
        &current_effect_params_runtime[fsm_ctx.setup_param_index];
    int32_t new_val = param_to_edit->value +
                      (encoder_event->steps *
                       param_to_edit->step); // value, steps, step are int32_t

    if (new_val < param_to_edit->min)
      new_val = param_to_edit->min; // min, max are int32_t
    if (new_val > param_to_edit->max)
      new_val = param_to_edit->max;

    if (param_to_edit->value != new_val) {
      param_to_edit->value = new_val;
      fsm_ctx.setup_has_changes = true;
      fsm_update_led_display();
      ESP_LOGD(TAG,
               "Effect '%s', Param '%s' (idx %" PRIu8 ") changed to %" PRIi32,
               current_active_effect->name, param_to_edit->name,
               fsm_ctx.setup_param_index, param_to_edit->value);
    }
    break;

  case MODE_SYSTEM_SETUP:
    if (fsm_ctx.system_setup_param_index <
        num_system_params) { // system_setup_param_index, num_system_params are
                             // uint8_t
      system_param_t *param =
          &system_params_temp_copy[fsm_ctx.system_setup_param_index];
      int32_t new_sys_val =
          param->value + (encoder_event->steps *
                          param->step); // value, steps, step are int32_t

      if (new_sys_val < param->min)
        new_sys_val = param->min; // min, max are int32_t
      if (new_sys_val > param->max)
        new_sys_val = param->max;

      if (param->value != new_sys_val) {
        param->value = new_sys_val;
        fsm_ctx.system_setup_has_changes = true;
        ESP_LOGI(TAG, "System Param '%s' temp value changed to %" PRId32,
                 param->name, param->value);
      }
    } else {
      ESP_LOGE(TAG,
               "system_setup_param_index %" PRIu8
               " is out of bounds (max %" PRIu8 ").",
               fsm_ctx.system_setup_param_index,
               (uint8_t)(num_system_params - 1));
    }
    break;

  default:
    ESP_LOGW(TAG, "Unknown state in encoder processing: %d",
             (int)fsm_ctx.current_state); // enum
    fsm_ctx.stats.invalid_events++;       // uint32_t
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

// Processa eventos ESP-NOW
static esp_err_t fsm_process_espnow_event(const espnow_event_t *espnow_event,
                                          uint32_t timestamp) {
  if (espnow_event == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  // data_len is uint16_t, timestamp is uint32_t
  ESP_LOGD(TAG,
           "ESP-NOW event: %" PRIu16
           " bytes from %02x:%02x:%02x:%02x:%02x:%02x, timestamp %" PRIu32,
           espnow_event->data_len, espnow_event->mac_addr[0],
           espnow_event->mac_addr[1], espnow_event->mac_addr[2],
           espnow_event->mac_addr[3], espnow_event->mac_addr[4],
           espnow_event->mac_addr[5], timestamp);

  // TODO: Implementar processamento de comandos ESP-NOW.
  // This is out of scope for the current FSM refactoring related to local UI
  // and LED control. Actual ESP-NOW command processing logic would be
  // implemented here when that feature is developed. Examples: Remote control
  // of power, brightness, effect selection, parameter adjustment, etc.
  // Synchronization commands between multiple devices.

  return ESP_OK;
}

// Faz transição entre estados
static esp_err_t fsm_transition_to_state(fsm_state_t new_state) {
  if (new_state == fsm_ctx.current_state) {
    return ESP_OK; // Já está no estado desejado
  }

  ESP_LOGI(TAG, "State transition: %d -> %d", (int)fsm_ctx.current_state,
           (int)new_state); // enums

  fsm_ctx.previous_state = fsm_ctx.current_state;
  fsm_ctx.current_state = new_state;
  fsm_ctx.state_entry_time = fsm_get_current_time_ms(); // uint32_t
  fsm_ctx.stats.state_transitions++;                    // uint32_t

  // Ações específicas na entrada de cada estado
  switch (new_state) {
  case MODE_DISPLAY:
    if (current_active_effect == NULL ||
        current_active_effect->id != fsm_ctx.current_effect ||
        (current_active_effect->param_count > 0 &&
         current_effect_params_runtime == NULL)) {
      ESP_LOGI(TAG,
               "Transition to DISPLAY: Reloading params for current effect ID "
               "%" PRIu8 ".",
               fsm_ctx.current_effect); // uint8_t
      fsm_load_effect_params(fsm_ctx.current_effect);
    }
    fsm_perform_visual_feedback(FEEDBACK_ENTERING_DISPLAY_MODE);
    fsm_update_led_display();
    break;

  case MODE_EFFECT_SELECT:
    fsm_perform_visual_feedback(FEEDBACK_ENTERING_EFFECT_SELECT);
    if (num_defined_effects > 0) {
      if (current_active_effect == NULL ||
          current_active_effect->id != fsm_ctx.current_effect ||
          (current_active_effect->param_count > 0 &&
           current_effect_params_runtime == NULL)) {
        ESP_LOGI(TAG,
                 "Transition to EFFECT_SELECT: Ensuring params for initial "
                 "effect ID %" PRIu8 " are loaded for preview.",
                 fsm_ctx.current_effect); // uint8_t
        fsm_load_effect_params(fsm_ctx.current_effect);
      }
      fsm_update_led_display();
    } else {
      ESP_LOGW(TAG, "Transition to EFFECT_SELECT: No effects defined. Display "
                    "will be blank or off.");
      led_controller_clear();
      led_controller_refresh();
    }
    break;

  case MODE_EFFECT_SETUP:
    fsm_ctx.setup_effect_index = fsm_ctx.current_effect;
    ESP_LOGI(TAG, "Transition to EFFECT_SETUP for effect ID %" PRIu8 ".",
             fsm_ctx.setup_effect_index); // uint8_t

    if (current_active_effect == NULL ||
        current_active_effect->id != fsm_ctx.setup_effect_index ||
        (current_active_effect->param_count > 0 &&
         current_effect_params_runtime == NULL)) {
      ESP_LOGI(TAG, "Loading params for effect ID %" PRIu8 " to be set up.",
               fsm_ctx.setup_effect_index); // uint8_t
      fsm_load_effect_params(fsm_ctx.setup_effect_index);
    } else {
      ESP_LOGI(
          TAG, "Params for effect %s (ID %" PRIu8 ") already loaded for setup.",
          current_active_effect->name, current_active_effect->id); // uint8_t
    }

    if (current_active_effect != NULL &&
        current_active_effect->id == fsm_ctx.setup_effect_index) {
      if (current_active_effect->param_count == 0) {
        ESP_LOGW(
            TAG,
            "Effect %s (ID %" PRIu8
            ") has no parameters. Cannot enter setup. Reverting to DISPLAY.",
            current_active_effect->name, current_active_effect->id); // uint8_t
        fsm_ctx.current_state = fsm_ctx.previous_state;
        fsm_transition_to_state(MODE_DISPLAY);
        return ESP_FAIL;
      }
      fsm_ctx.setup_param_index = 0;
      fsm_ctx.setup_has_changes = false;
      fsm_perform_visual_feedback(FEEDBACK_ENTERING_EFFECT_SETUP);
      ESP_LOGI(TAG,
               "Starting setup for effect: %s. Editing param: %s (idx %" PRIu8
               ")", // uint8_t for idx
               current_active_effect->name,
               (current_effect_params_runtime)
                   ? current_effect_params_runtime[0].name
                   : "N/A",
               fsm_ctx.setup_param_index); // Corrected: use setup_param_index
                                           // for the log
    } else {
      ESP_LOGE(TAG,
               "Failed to enter setup: Effect ID %" PRIu8
               " could not be loaded. Reverting to DISPLAY.",
               fsm_ctx.setup_effect_index); // uint8_t
      fsm_ctx.current_state = fsm_ctx.previous_state;
      fsm_transition_to_state(MODE_DISPLAY);
      return ESP_FAIL;
    }
    fsm_update_led_display();
    break;

  case MODE_SYSTEM_SETUP:
    fsm_perform_visual_feedback(FEEDBACK_ENTERING_SYSTEM_SETUP);
    memcpy(system_params_temp_copy, system_params_array,
           sizeof(system_params_array));
    fsm_ctx.system_setup_param_index = 0;
    fsm_ctx.system_setup_has_changes = false;
    ESP_LOGI(TAG, "Entering System Setup. Editing: %s (Value: %" PRId32 ")",
             system_params_temp_copy[fsm_ctx.system_setup_param_index].name,
             system_params_temp_copy[fsm_ctx.system_setup_param_index]
                 .value); // value is int32_t
    break;

  default:
    ESP_LOGW(TAG, "Unknown state in transition: %d", (int)new_state); // enum
    return ESP_ERR_INVALID_ARG;
  }

  return ESP_OK;
}

// Verifica timeout de modo (volta para DISPLAY se ficar muito tempo em outros
// estados)
static void
fsm_check_mode_timeout(uint32_t current_time) { // current_time is uint32_t
  if (fsm_ctx.current_state == MODE_DISPLAY) {
    return;
  }

  uint32_t time_in_state =
      current_time - fsm_ctx.state_entry_time; // Both uint32_t
  if (time_in_state >
      fsm_ctx.config.mode_timeout_ms) { // mode_timeout_ms is uint32_t
    ESP_LOGI(TAG,
             "Mode timeout in state %d, returning to display (timeout: %" PRIu32
             "ms, actual: %" PRIu32 "ms)",
             (int)fsm_ctx.current_state, fsm_ctx.config.mode_timeout_ms,
             time_in_state); // enum, uint32_t, uint32_t
    fsm_transition_to_state(MODE_DISPLAY);
  }
}

// Atualiza display dos LEDs baseado no estado atual
static void fsm_update_led_display(void) {
  if (!fsm_ctx.led_strip_on) {
    led_controller_clear();
    led_controller_refresh();
    ESP_LOGD(TAG, "LEDs turned OFF. Cleared and refreshed.");
    return;
  }

  if (current_active_effect == NULL || current_effect_params_runtime == NULL) {
    if (num_defined_effects > 0 && current_active_effect == NULL) {
      ESP_LOGW(TAG,
               "No active effect or params, attempting to load default effect "
               "ID %" PRIu8,
               fsm_ctx.current_effect); // uint8_t
      fsm_load_effect_params(fsm_ctx.current_effect);
      if (current_active_effect == NULL ||
          (current_active_effect->param_count > 0 &&
           current_effect_params_runtime == NULL)) {
        ESP_LOGE(
            TAG,
            "Failed to load default effect or its params. Clearing strip.");
        led_controller_clear();
        led_controller_refresh();
        return;
      }
    } else if (num_defined_effects == 0) {
      ESP_LOGW(TAG, "No effects defined. Clearing strip.");
      led_controller_clear();
      led_controller_refresh();
      return;
    }
    if (current_active_effect != NULL &&
        current_active_effect->param_count > 0 &&
        current_effect_params_runtime == NULL) {
      ESP_LOGE(TAG,
               "Effect %s (ID %" PRIu8
               ") has params but runtime_params is NULL. Clearing strip.",
               current_active_effect->name,
               current_active_effect->id); // uint8_t
      led_controller_clear();
      led_controller_refresh();
      return;
    }
  }

  ESP_LOGD(TAG,
           "Updating LED display - Effect: %s (ID: %" PRIu8
           "), Brightness: %" PRIu8 ", Power: %s", // uint8_t, uint8_t
           current_active_effect ? current_active_effect->name : "None",
           fsm_ctx.current_effect, fsm_ctx.global_brightness,
           fsm_ctx.led_strip_on ? "ON" : "OFF");

  switch (current_active_effect->id) { // id is uint8_t
  case 0:                              // Static Color
    if (current_active_effect->param_count >= 2 &&
        current_effect_params_runtime != NULL) {
      uint16_t hue =
          current_effect_params_runtime[0]
              .value; // value is int32_t, but hue for set_pixel_hsv is uint16_t
      uint8_t sat =
          current_effect_params_runtime[1]
              .value; // value is int32_t, but sat for set_pixel_hsv is uint8_t
      for (uint32_t i = 0; i < LED_STRIP_NUM_LEDS;
           i++) { // LED_STRIP_NUM_LEDS is usually uint32_t or similar
        led_controller_set_pixel_hsv(i, hue, sat, 255);
      }
    } else {
      ESP_LOGE(TAG,
               "Static Color effect (ID %" PRIu8
               ") misconfigured or params not loaded.",
               current_active_effect->id); // uint8_t
      for (uint32_t i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        led_controller_set_pixel_hsv(i, 0, 0, 100);
      }
    }
    break;

  case 1: // Candle
    if (current_active_effect->param_count >= 1 &&
        current_effect_params_runtime != NULL) {
      uint16_t base_hue = current_effect_params_runtime[0].value; // As above
      for (uint32_t i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        led_controller_set_pixel_hsv(i, base_hue, 220, 255);
      }
    } else {
      ESP_LOGE(TAG,
               "Candle effect (ID %" PRIu8
               ") misconfigured or params not loaded.",
               current_active_effect->id); // uint8_t
      for (uint32_t i = 0; i < LED_STRIP_NUM_LEDS; i++) {
        led_controller_set_pixel_hsv(i, 30, 200, 255);
      }
    }
    break;

  default:
    ESP_LOGW(TAG, "Effect ID %" PRIu8 " not implemented. Turning LEDs off.",
             current_active_effect->id); // uint8_t
    led_controller_clear();
    break;
  }
  led_controller_refresh();
}

// Obtém timestamp atual em millisegundos
static uint32_t fsm_get_current_time_ms(void) {
  return (uint32_t)(esp_timer_get_time() /
                    1000); // Returns uint64_t, cast to uint32_t
}

// Helper function to load parameters for a given effect ID
static void fsm_load_effect_params(uint8_t effect_id) { // effect_id is uint8_t
  const led_effect_t *effect = led_effects_get_by_id(effect_id);
  if (effect == NULL) {
    ESP_LOGE(TAG, "Effect ID %" PRIu8 " not found.", effect_id); // uint8_t
    return;
  }

  if (current_effect_params_runtime != NULL) {
    free(current_effect_params_runtime);
    current_effect_params_runtime = NULL;
  }

  current_active_effect = effect;

  if (effect->param_count > 0) { // param_count is uint8_t
    current_effect_params_runtime =
        malloc(sizeof(effect_param_t) * effect->param_count);
    if (current_effect_params_runtime) {
      memcpy(current_effect_params_runtime, effect->params,
             sizeof(effect_param_t) * effect->param_count);
      ESP_LOGI(TAG,
               "Loaded %" PRIu8 " parameters for effect: %s (ID: %" PRIu8 ")",
               effect->param_count, effect->name,
               effect->id); // uint8_t for counts and ID
    } else {
      ESP_LOGE(TAG,
               "Failed to allocate memory for runtime parameters of effect %s",
               effect->name);
      current_active_effect = NULL;
    }
  } else {
    ESP_LOGI(TAG, "Effect %s (ID: %" PRIu8 ") has no parameters.", effect->name,
             effect->id); // uint8_t for ID
  }
}

// --- Visual Feedback Implementation (Simplified to Logs) ---
static void fsm_perform_visual_feedback(
    fsm_feedback_event_t feedback_type) { // feedback_type is enum
  const char *event_name = "UNKNOWN";
  switch (feedback_type) {
  // ... cases ...
  default:
    ESP_LOGW(TAG, "Unknown visual feedback type: %d", (int)feedback_type);
    return; // enum
  }
  ESP_LOGI(TAG, "Visual Feedback Event: %s", event_name);
}

// --- Apply System Settings ---
static void fsm_apply_system_settings(void) {
  ESP_LOGI(TAG, "Applying system settings...");
  if (system_params_array[0].value * 1000 !=
      fsm_ctx.config
          .mode_timeout_ms) { // value is int32_t, mode_timeout_ms is uint32_t
    fsm_ctx.config.mode_timeout_ms = system_params_array[0].value * 1000;
    ESP_LOGI(TAG, "Mode timeout updated to %" PRIu32 " ms",
             fsm_ctx.config.mode_timeout_ms); // uint32_t
  }
  uint8_t new_brightness =
      (uint8_t)system_params_array[1].value; // value is int32_t
  if (new_brightness !=
      fsm_ctx.global_brightness) { // global_brightness is uint8_t
    fsm_ctx.global_brightness = new_brightness;
    led_controller_set_brightness(fsm_ctx.global_brightness);
    ESP_LOGI(TAG, "Global brightness updated to %" PRIu8 " via system settings",
             fsm_ctx.global_brightness); // uint8_t
    fsm_update_led_display();
  }
  ESP_LOGI(
      TAG,
      "System settings applied from system_params_array. (NVS save is TODO)");
  fsm_ctx.system_setup_has_changes = false;
}
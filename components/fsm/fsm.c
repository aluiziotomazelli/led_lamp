#include "fsm.h"
#include "esp_timer.h"
#include "led_controller.h"  // Para controle dos LEDs
#include "project_config.h"  // For LED_STRIP_NUM_LEDS etc.
#include <string.h>          // For memset, memcpy
#include <stdlib.h>          // For malloc, free
#include <inttypes.h>        // For PRId32 macro

#include "led_effects.h"     // For led_effect_t, effect_param_t
#include "led_strip_types.h" // For LED_PIXEL_FORMAT_GRB, LED_MODEL_SK6812
#include "driver/spi_common.h" // For SPI_CLK_SRC_DEFAULT

static const char *TAG = "FSM";

// LED Effects state (global to FSM component)
static const led_effect_t* current_active_effect = NULL;
static effect_param_t* current_effect_params_runtime = NULL; // Mutable copy for runtime adjustments
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
static system_param_t system_params_temp_copy[MAX_SYSTEM_PARAMS]; // For "cancel" functionality

// Estrutura de contexto da FSM
typedef struct {
    // Configuração
    QueueHandle_t event_queue;          ///< Handle da queue de eventos
    TaskHandle_t fsm_task_handle;       ///< Handle da task da FSM
    fsm_mode_t config;                ///< Configurações
    
    // Estado atual
    fsm_state_t current_state;          ///< Estado atual da FSM
    fsm_state_t previous_state;         ///< Estado anterior (para rollback)
    uint32_t state_entry_time;          ///< Timestamp de entrada no estado atual
    
    // Dados do sistema
    uint8_t current_effect;             ///< ID do efeito atualmente selecionado
    uint8_t previous_effect_id;         ///< ID do efeito anterior (para rollback em MODE_EFFECT_SELECT)
    uint8_t global_brightness;          ///< Brilho global (0-254)
    bool led_strip_on;                  ///< Estado da fita LED
    
    // Setup state
    uint8_t setup_param_index;          ///< Parâmetro sendo editado no setup do efeito
    uint8_t setup_effect_index;         ///< ID do efeito sendo editado no setup
    bool setup_has_changes;             ///< Flag de mudanças não salvas no setup do efeito
    
    // System Setup state
    uint8_t system_setup_param_index;   ///< Parâmetro do sistema sendo editado
    bool system_setup_has_changes;      ///< Flag de mudanças não salvas no setup do sistema

    // Controle
    bool initialized;                   ///< Flag de inicialização
    bool running;                       ///< Flag de execução
    
    // Estatísticas
    fsm_stats_t stats;                  ///< Estatísticas da FSM
} fsm_context_t;

static fsm_context_t fsm_ctx = {0};

// Declarações das funções internas
static void fsm_task(void *pvParameters);
static esp_err_t fsm_process_integrated_event(const integrated_event_t *event);
static esp_err_t fsm_process_button_event(const button_event_t *button_event, uint32_t timestamp);
static esp_err_t fsm_process_encoder_event(const encoder_event_t *encoder_event, uint32_t timestamp);
static esp_err_t fsm_process_espnow_event(const espnow_event_t *espnow_event, uint32_t timestamp);
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
    FEEDBACK_SAVED_PARAM, // When a single parameter is confirmed by click in setup
    FEEDBACK_SAVED_EFFECT_SETTINGS, // When exiting setup with changes via long click or last param click
    FEEDBACK_CANCEL_SETUP, // When exiting setup via double click (changes discarded)
    FEEDBACK_EFFECT_SELECTED, // When an effect is chosen in select mode by click
    FEEDBACK_REVERT_EFFECT_SELECTION, // When selection is cancelled by double click
    FEEDBACK_POWER_ON,
    FEEDBACK_POWER_OFF,
    FEEDBACK_SYSTEM_PARAM_SELECTED,   // Click in system setup to go to next param
    FEEDBACK_SYSTEM_SETTINGS_SAVED,   // System settings applied
    FEEDBACK_SYSTEM_SETUP_CANCELLED,  // Exited system setup, changes for this session discarded
    FEEDBACK_EXIT_SYSTEM_SETUP,       // Exited system setup (changes might have been saved)
} fsm_feedback_event_t;

static void fsm_perform_visual_feedback(fsm_feedback_event_t feedback_type);
static void fsm_apply_system_settings(void); // Forward declaration for fsm_apply_system_settings


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
    fsm_ctx.current_effect = 0; // Default to first effect ID
    fsm_ctx.previous_effect_id = 0; // Initialize to default effect ID
    fsm_ctx.global_brightness = 128;  // 50% de brilho inicial
    fsm_ctx.led_strip_on = true;
    fsm_ctx.setup_param_index = 0;
    fsm_ctx.setup_effect_index = 0; // Will be set to current_effect when entering setup mode
    fsm_ctx.setup_has_changes = false;

    fsm_ctx.system_setup_param_index = 0;
    fsm_ctx.system_setup_has_changes = false;

    // TODO: Load system_params_array from NVS here in a real implementation
    // For now, apply default values from the static array for config.mode_timeout_ms
    // This ensures that if fsm_ctx.config was passed in, its mode_timeout_ms is respected
    // unless overwritten by system settings later or if config was NULL.
    if (config == NULL) { // Only apply from system_params_array if no config was supplied
        fsm_ctx.config.mode_timeout_ms = system_params_array[0].value * 1000;
    }
    // Note: Default brightness from system_params_array[1] is not automatically applied to fsm_ctx.global_brightness here.
    // It's a setting that can be manually applied via system setup, or could be used at initial cold boot.
    
    // Limpa estatísticas
    memset(&fsm_ctx.stats, 0, sizeof(fsm_stats_t));
    fsm_ctx.stats.current_state = fsm_ctx.current_state;

    // Initialize LED controller
    led_controller_config_t led_cfg = {
        .num_leds = LED_STRIP_NUM_LEDS,             // From project_config.h
        .spi_host = LED_STRIP_SPI_HOST,             // From project_config.h
        .clk_speed_hz = LED_STRIP_SPI_CLK_SPEED_HZ, // From project_config.h
        .spi_mosi_gpio = LED_STRIP_GPIO_MOSI,         // From project_config.h
        .spi_sclk_gpio = LED_STRIP_GPIO_SCLK,         // From project_config.h
        .pixel_format = LED_PIXEL_FORMAT_GRB,       // Using direct value
        .model = LED_MODEL_SK6812,                  // Using direct value
        .spi_clk_src = SPI_CLK_SRC_DEFAULT          // Using direct value
    };
    esp_err_t ret_led = led_controller_init(&led_cfg);
    if (ret_led != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize LED controller: %s", esp_err_to_name(ret_led));
        // Handle error, perhaps prevent FSM from starting fully
        // For now, we'll continue, but LEDs won't work.
    } else {
        ESP_LOGI(TAG, "LED controller initialized.");
        // Load available effects
        const led_effect_t* effects_array = led_effects_get_all(&num_defined_effects);
        if (num_defined_effects > 0 && effects_array != NULL) {
            ESP_LOGI(TAG, "%d LED effects defined. Defaulting to effect ID %d.", num_defined_effects, fsm_ctx.current_effect);
            fsm_load_effect_params(fsm_ctx.current_effect); // Load params for the default effect
            if (current_active_effect == NULL) {
                 ESP_LOGE(TAG, "Failed to load default effect %d.", fsm_ctx.current_effect);
                 // Potentially try to load the absolute first effect if default fails
                 if (fsm_ctx.current_effect != effects_array[0].id) {
                    fsm_ctx.current_effect = effects_array[0].id;
                    fsm_load_effect_params(fsm_ctx.current_effect);
                 }
                 if (current_active_effect == NULL && num_defined_effects > 0) { // Still failed
                    ESP_LOGE(TAG, "Failed to load even the first defined effect. LED effects will be unavailable.");
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
    BaseType_t result = xTaskCreate(
        fsm_task,
        "fsm_task",
        fsm_ctx.config.task_stack_size,
        NULL,
        fsm_ctx.config.task_priority,
        &fsm_ctx.fsm_task_handle
    );

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

    ESP_LOGI(TAG, "FSM initialized successfully in state %d", fsm_ctx.current_state);
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
        ESP_LOGE(TAG, "Failed to deinitialize LED controller: %s", esp_err_to_name(deinit_ret));
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

bool fsm_is_running(void) {
    return fsm_ctx.initialized && fsm_ctx.running;
}

fsm_state_t fsm_get_current_state(void) {
    return fsm_ctx.current_state;
}

uint8_t fsm_get_current_effect(void) {
    return fsm_ctx.current_effect;
}

uint8_t fsm_get_global_brightness(void) {
    return fsm_ctx.global_brightness;
}

bool fsm_is_led_strip_on(void) {
    return fsm_ctx.led_strip_on;
}

esp_err_t fsm_get_stats(fsm_stats_t *stats) {
    if (stats == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    fsm_ctx.stats.current_state = fsm_ctx.current_state;
    fsm_ctx.stats.time_in_current_state = fsm_get_current_time_ms() - fsm_ctx.state_entry_time;
    *stats = fsm_ctx.stats;
    return ESP_OK;
}

void fsm_reset_stats(void) {
    memset(&fsm_ctx.stats, 0, sizeof(fsm_stats_t));
    fsm_ctx.stats.current_state = fsm_ctx.current_state;
    ESP_LOGI(TAG, "Statistics reset");
}

esp_err_t fsm_force_state(fsm_state_t new_state) {
    if (new_state >= MODE_SYSTEM_SETUP + 1) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGW(TAG, "Forcing state transition from %d to %d", fsm_ctx.current_state, new_state);
    return fsm_transition_to_state(new_state);
}

// Task principal da FSM
static void fsm_task(void *pvParameters) {
    integrated_event_t event;
    uint32_t current_time;

    ESP_LOGI(TAG, "FSM task started");

    while (fsm_ctx.running) {
        // Espera por eventos na queue
        if (xQueueReceive(fsm_ctx.event_queue, &event, fsm_ctx.config.queue_timeout_ms) == pdTRUE) {
            fsm_ctx.stats.events_processed++;
            
            esp_err_t ret = fsm_process_integrated_event(&event);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Error processing event: %s", esp_err_to_name(ret));
            }
        } else {
            // Timeout na queue - verifica timeouts do sistema
            fsm_ctx.stats.queue_timeouts++;
        }

        // Verifica timeout de modo (volta para DISPLAY se ficar muito tempo em outros modos)
        current_time = fsm_get_current_time_ms();
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
            ret = fsm_process_button_event(&event->data.button, event->timestamp);
            fsm_ctx.stats.button_events++;
            break;

        case EVENT_SOURCE_ENCODER:
            ret = fsm_process_encoder_event(&event->data.encoder, event->timestamp);
            fsm_ctx.stats.encoder_events++;
            break;

        case EVENT_SOURCE_ESPNOW:
            ret = fsm_process_espnow_event(&event->data.espnow, event->timestamp);
            fsm_ctx.stats.espnow_events++;
            break;

        default:
            ESP_LOGW(TAG, "Unknown event source: %d", event->source);
            fsm_ctx.stats.invalid_events++;
            ret = ESP_ERR_INVALID_ARG;
            break;
    }

    return ret;
}

// Processa eventos de botão baseado no estado atual
static esp_err_t fsm_process_button_event(const button_event_t *button_event, uint32_t timestamp) {
    if (button_event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Button event: %d in state %d", button_event->type, fsm_ctx.current_state);

    switch (fsm_ctx.current_state) {
        case MODE_DISPLAY:
            switch (button_event->type) {
                case BUTTON_CLICK:
                    // Liga/desliga a fita LED
                    fsm_ctx.led_strip_on = !fsm_ctx.led_strip_on;
                    fsm_perform_visual_feedback(fsm_ctx.led_strip_on ? FEEDBACK_POWER_ON : FEEDBACK_POWER_OFF);
                    fsm_update_led_display();
                    ESP_LOGI(TAG, "LED strip %s", fsm_ctx.led_strip_on ? "ON" : "OFF");
                    break;
                    
                case BUTTON_DOUBLE_CLICK:
                    // Entra no modo seleção de efeito
                    if (num_defined_effects > 0) { // Only if effects are available
                        fsm_ctx.previous_effect_id = fsm_ctx.current_effect; // Store current effect before switching
                        fsm_transition_to_state(MODE_EFFECT_SELECT);
                        // Visual feedback for entering select mode is in fsm_transition_to_state
                    } else {
                        ESP_LOGW(TAG, "No effects defined, cannot enter EFFECT_SELECT mode.");
                    }
                    break;
                    
                case BUTTON_LONG_CLICK:
                    // Entra no setup do efeito atual
                    if (current_active_effect != NULL && current_active_effect->param_count > 0) {
                        fsm_ctx.setup_effect_index = fsm_ctx.current_effect;
                        if (current_active_effect->id != fsm_ctx.current_effect) {
                             ESP_LOGI(TAG, "Loading params for effect %d before setup.", fsm_ctx.current_effect);
                             fsm_load_effect_params(fsm_ctx.current_effect); // This also sets current_active_effect
                        }
                        // Ensure current_active_effect is indeed the one we are setting up
                        if (current_active_effect && current_active_effect->id == fsm_ctx.setup_effect_index) {
                            fsm_ctx.setup_param_index = 0;
                            fsm_ctx.setup_has_changes = false;
                            fsm_transition_to_state(MODE_EFFECT_SETUP);
                            // Visual feedback for entering setup mode is in fsm_transition_to_state
                        } else {
                            ESP_LOGE(TAG, "Failed to load effect %d for setup. Staying in Display mode.", fsm_ctx.current_effect);
                        }
                    } else {
                         ESP_LOGW(TAG, "Current effect '%s' has no parameters or no effect active; cannot enter EFFECT_SETUP.", current_active_effect ? current_active_effect->name : "None");
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
                    // Seleciona o efeito atualmente previewed (fsm_ctx.current_effect) e volta para exibição
                    fsm_perform_visual_feedback(FEEDBACK_EFFECT_SELECTED);
                    fsm_transition_to_state(MODE_DISPLAY);
                    ESP_LOGI(TAG, "Effect %s (ID: %d) selected.", current_active_effect ? current_active_effect->name : "None", fsm_ctx.current_effect);
                    break;

                case BUTTON_DOUBLE_CLICK: // Revert to previous_effect_id
                    ESP_LOGI(TAG, "Effect selection: Double click - reverting to effect ID %d.", fsm_ctx.previous_effect_id);
                    fsm_ctx.current_effect = fsm_ctx.previous_effect_id; // Set context's current_effect first
                    fsm_load_effect_params(fsm_ctx.current_effect); // Load its params, this also updates current_active_effect
                    fsm_perform_visual_feedback(FEEDBACK_REVERT_EFFECT_SELECTION);
                    fsm_transition_to_state(MODE_DISPLAY);
                    break;
                    
                case BUTTON_LONG_CLICK:
                    // Cancels selection, reverts to the effect that was active *before* entering MODE_EFFECT_SELECT.
                    ESP_LOGI(TAG, "Exiting effect selection (Long Click). Reverting to effect ID %d.", fsm_ctx.previous_effect_id);
                    fsm_ctx.current_effect = fsm_ctx.previous_effect_id;
                    fsm_load_effect_params(fsm_ctx.current_effect);
                    // No specific feedback for this action, or could be same as REVERT_EFFECT_SELECTION
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
                    if (current_active_effect != NULL && current_effect_params_runtime != NULL && current_active_effect->param_count > 0) {
                        fsm_perform_visual_feedback(FEEDBACK_SAVED_PARAM); // Feedback for saving current param
                        fsm_ctx.setup_param_index++;
                        if (fsm_ctx.setup_param_index >= current_active_effect->param_count) {
                            ESP_LOGI(TAG, "All parameters for '%s' configured.", current_active_effect->name);
                            if (fsm_ctx.setup_has_changes) {
                                ESP_LOGI(TAG, "Changes to '%s' applied (runtime).", current_active_effect->name);
                                // NVS Save would go here
                                fsm_ctx.setup_has_changes = false;
                            }
                            fsm_perform_visual_feedback(FEEDBACK_SAVED_EFFECT_SETTINGS);
                            fsm_transition_to_state(MODE_DISPLAY);
                        } else {
                            ESP_LOGI(TAG, "Moving to edit param %d ('%s') for effect '%s'",
                                     fsm_ctx.setup_param_index,
                                     current_effect_params_runtime[fsm_ctx.setup_param_index].name,
                                     current_active_effect->name);
                        }
                    } else {
                        ESP_LOGW(TAG, "No effect/params in setup, returning to display.");
                        fsm_transition_to_state(MODE_DISPLAY);
                    }
                    break;

                case BUTTON_DOUBLE_CLICK: // Cancel setup for current effect
                    ESP_LOGI(TAG, "Effect setup: Double click - cancelling changes for effect ID %d.", fsm_ctx.setup_effect_index);
                    fsm_ctx.current_effect = fsm_ctx.setup_effect_index; // Ensure this is the effect we are reverting
                    fsm_load_effect_params(fsm_ctx.current_effect); // Reload original params from const definition
                    fsm_perform_visual_feedback(FEEDBACK_CANCEL_SETUP);
                    fsm_transition_to_state(MODE_DISPLAY);
                    break;
                    
                case BUTTON_LONG_CLICK:
                    ESP_LOGI(TAG, "Exiting effect setup for '%s'.", current_active_effect ? current_active_effect->name : "N/A");
                    if (fsm_ctx.setup_has_changes) {
                        ESP_LOGI(TAG, "Runtime changes to '%s' are kept (NVS TODO).", current_active_effect ? current_active_effect->name : "N/A");
                        // NVS Save would go here
                        fsm_perform_visual_feedback(FEEDBACK_SAVED_EFFECT_SETTINGS);
                        fsm_ctx.setup_has_changes = false;
                    } else {
                        // fsm_perform_visual_feedback(FEEDBACK_EXIT_SETUP_NO_CHANGES); // Optional distinct feedback
                    }
                    // Ensure fsm_ctx.current_effect is the one that was being edited.
                    fsm_ctx.current_effect = fsm_ctx.setup_effect_index;
                    // Parameters in current_effect_params_runtime are already what they should be (either changed or not).
                    // If no changes, fsm_load_effect_params isn't strictly needed here but harmless if called by MODE_DISPLAY transition.
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
                            memcpy(system_params_array, system_params_temp_copy, sizeof(system_params_array));
                            fsm_apply_system_settings(); // This also resets system_setup_has_changes
                            fsm_perform_visual_feedback(FEEDBACK_SYSTEM_SETTINGS_SAVED);
                        } else {
                            // No specific feedback if no changes, just exiting.
                        }
                        fsm_transition_to_state(MODE_DISPLAY);
                    } else {
                        ESP_LOGI(TAG, "System Setup: Now editing: %s (Value: %" PRId32 ")",
                                 system_params_temp_copy[fsm_ctx.system_setup_param_index].name,
                                 system_params_temp_copy[fsm_ctx.system_setup_param_index].value);
                    }
                    break;

                case BUTTON_DOUBLE_CLICK: // Cancel without saving current session's changes
                    ESP_LOGI(TAG, "System setup cancelled via double click. Changes in this session discarded.");
                    fsm_perform_visual_feedback(FEEDBACK_SYSTEM_SETUP_CANCELLED);
                    // system_params_array remains untouched by this session's changes because we only modified system_params_temp_copy.
                    // No need to copy back from a non-existent pristine backup for this session.
                    fsm_transition_to_state(MODE_DISPLAY);
                    break;
                    
                case BUTTON_LONG_CLICK: // Save and Exit
                    ESP_LOGI(TAG, "Exiting System Setup (Long Click).");
                    if (fsm_ctx.system_setup_has_changes) {
                        ESP_LOGI(TAG, "Applying changes from system setup.");
                        memcpy(system_params_array, system_params_temp_copy, sizeof(system_params_array));
                        fsm_apply_system_settings(); // This also resets system_setup_has_changes
                        fsm_perform_visual_feedback(FEEDBACK_SYSTEM_SETTINGS_SAVED);
                    } else {
                        fsm_perform_visual_feedback(FEEDBACK_EXIT_SYSTEM_SETUP); // Exiting without changes to save
                    }
                    fsm_transition_to_state(MODE_DISPLAY);
                    break;
                    
                default:
                    break;
            }
            break;

        default:
            ESP_LOGW(TAG, "Unknown state in button processing: %d", fsm_ctx.current_state);
            fsm_ctx.stats.invalid_events++;
            return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

// Processa eventos de encoder baseado no estado atual
static esp_err_t fsm_process_encoder_event(const encoder_event_t *encoder_event, uint32_t timestamp) {
    if (encoder_event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "Encoder event: %d steps in state %d", encoder_event->steps, fsm_ctx.current_state);

    switch (fsm_ctx.current_state) {
        case MODE_DISPLAY:
            // Ajusta brilho global
            int32_t new_brightness = (int32_t)fsm_ctx.global_brightness + encoder_event->steps;
            if (new_brightness < 0) new_brightness = 0;
            if (new_brightness > 255) new_brightness = 255; // Max brightness for led_controller
            fsm_ctx.global_brightness = (uint8_t)new_brightness;

            led_controller_set_brightness(fsm_ctx.global_brightness);
            fsm_update_led_display(); // Re-render with new global brightness
            ESP_LOGD(TAG, "Brightness adjusted to %d", fsm_ctx.global_brightness);
            break;

        case MODE_EFFECT_SELECT:
            if (num_defined_effects == 0) break; // No effects to select

            int32_t new_effect_idx_signed = fsm_ctx.current_effect + encoder_event->steps;
            uint8_t new_effect_id;

            // Find the actual ID from the array of effects based on index
            // This assumes effects are somewhat contiguous or we iterate to find the Nth effect.
            // For simplicity, let's assume IDs are 0 to num_defined_effects - 1 for now.
            // A more robust way would be to have an array of effect IDs.
            if (new_effect_idx_signed < 0) {
                new_effect_id = (num_defined_effects > 0) ? (num_defined_effects - 1) : 0;
            } else if (new_effect_idx_signed >= num_defined_effects) {
                new_effect_id = 0;
            } else {
                new_effect_id = (uint8_t)new_effect_idx_signed;
            }

            // Check if this ID actually exists (e.g. if IDs are not contiguous 0..N-1)
            const led_effect_t* temp_effect = led_effects_get_by_id(new_effect_id);
            if (temp_effect == NULL && num_defined_effects > 0) { // Try to find next valid if current is bad
                 ESP_LOGW(TAG, "Effect ID %d from index %d not found, trying to find next valid.", new_effect_id, new_effect_idx_signed);
                 // This simple wrap-around might not be ideal if IDs are sparse.
                 // For now, we'll rely on the current simple ID scheme (0, 1, ...).
                 // If current_effect was 0 and we decrement, it might go to num_effects-1.
                 // If it was num_effects-1 and we increment, it goes to 0.
                 // This logic needs to align with how effects are stored and ID'd.
                 // Assuming current_effect stores the ID, and led_effects_get_by_id is the source of truth.
                 // Let's find the current effect's index in the all_effects array to do proper circular navigation.
                const led_effect_t* all_fx = led_effects_get_all(NULL); // Get array without caring for count here
                int current_known_idx = -1;
                for(uint8_t i=0; i < num_defined_effects; ++i) {
                    if(all_fx[i].id == fsm_ctx.current_effect) {
                        current_known_idx = i;
                        break;
                    }
                }
                if(current_known_idx != -1) {
                    int next_idx = (current_known_idx + encoder_event->steps);
                    while(next_idx < 0) next_idx += num_defined_effects; // Ensure positive before modulo
                    next_idx %= num_defined_effects;
                    new_effect_id = all_fx[next_idx].id;
                } else { // current_effect ID not found in list? Fallback to first.
                    new_effect_id = (num_defined_effects > 0) ? all_fx[0].id : 0;
                }
            }


            if (new_effect_id != fsm_ctx.current_effect) {
                fsm_ctx.current_effect = new_effect_id;
                fsm_load_effect_params(fsm_ctx.current_effect); // Load new effect's parameters
                fsm_update_led_display();  // Show preview of the new effect
            }
            ESP_LOGD(TAG, "Effect selection preview: %s (ID: %d)", current_active_effect ? current_active_effect->name : "None", fsm_ctx.current_effect);
            break;

        case MODE_EFFECT_SETUP:
            if (current_active_effect == NULL || current_effect_params_runtime == NULL || current_active_effect->param_count == 0) {
                ESP_LOGW(TAG, "In MODE_EFFECT_SETUP but no active effect or parameters to adjust.");
                break; // No parameters to adjust
            }
            if (fsm_ctx.setup_param_index >= current_active_effect->param_count) {
                 ESP_LOGE(TAG, "setup_param_index %d out of bounds for effect %s (count %d)",
                    fsm_ctx.setup_param_index, current_active_effect->name, current_active_effect->param_count);
                break;
            }

            effect_param_t* param_to_edit = &current_effect_params_runtime[fsm_ctx.setup_param_index];
            int32_t new_val = param_to_edit->value + (encoder_event->steps * param_to_edit->step);

            if (new_val < param_to_edit->min) new_val = param_to_edit->min;
            if (new_val > param_to_edit->max) new_val = param_to_edit->max;

            if (param_to_edit->value != new_val) {
                param_to_edit->value = new_val;
                fsm_ctx.setup_has_changes = true;
                fsm_update_led_display(); // Show change in real-time
                ESP_LOGD(TAG, "Effect '%s', Param '%s' (idx %d) changed to %" PRIi32,
                         current_active_effect->name, param_to_edit->name, fsm_ctx.setup_param_index, param_to_edit->value);
            }
            break;

        case MODE_SYSTEM_SETUP:
            if (fsm_ctx.system_setup_param_index < num_system_params) {
                system_param_t* param = &system_params_temp_copy[fsm_ctx.system_setup_param_index];
                int32_t new_sys_val = param->value + (encoder_event->steps * param->step);

                if (new_sys_val < param->min) new_sys_val = param->min;
                if (new_sys_val > param->max) new_sys_val = param->max;

                if (param->value != new_sys_val) {
                    param->value = new_sys_val;
                    fsm_ctx.system_setup_has_changes = true;
                    ESP_LOGI(TAG, "System Param '%s' temp value changed to %" PRId32, param->name, param->value);

                    // Optional: Immediate feedback for some params, e.g., if editing a color or brightness related system setting.
                    // For "Mode Timeout (s)" and "Def Brightness", immediate visual feedback on main LED strip might be confusing.
                    // A small indicator LED or segment display would be better for this mode.
                    // For now, logging is the primary feedback.
                    // If system param was, e.g., a "setup mode indicator color", could call led_controller here.
                }
            } else {
                 ESP_LOGE(TAG, "system_setup_param_index %d is out of bounds (max %d).", fsm_ctx.system_setup_param_index, num_system_params -1);
            }
            break;

        default:
            ESP_LOGW(TAG, "Unknown state in encoder processing: %d", fsm_ctx.current_state);
            fsm_ctx.stats.invalid_events++;
            return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

// Processa eventos ESP-NOW
static esp_err_t fsm_process_espnow_event(const espnow_event_t *espnow_event, uint32_t timestamp) {
    if (espnow_event == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGD(TAG, "ESP-NOW event: %d bytes from %02x:%02x:%02x:%02x:%02x:%02x",
             espnow_event->data_len,
             espnow_event->mac_addr[0], espnow_event->mac_addr[1], espnow_event->mac_addr[2],
             espnow_event->mac_addr[3], espnow_event->mac_addr[4], espnow_event->mac_addr[5]);

    // TODO: Implementar processamento de comandos ESP-NOW.
    // This is out of scope for the current FSM refactoring related to local UI and LED control.
    // Actual ESP-NOW command processing logic would be implemented here when that feature is developed.
    // Examples: Remote control of power, brightness, effect selection, parameter adjustment, etc.
    // Synchronization commands between multiple devices.

    return ESP_OK;
}

// Faz transição entre estados
static esp_err_t fsm_transition_to_state(fsm_state_t new_state) {
    if (new_state == fsm_ctx.current_state) {
        return ESP_OK;  // Já está no estado desejado
    }

    ESP_LOGI(TAG, "State transition: %d -> %d", fsm_ctx.current_state, new_state);
    
    fsm_ctx.previous_state = fsm_ctx.current_state;
    fsm_ctx.current_state = new_state;
    fsm_ctx.state_entry_time = fsm_get_current_time_ms();
    fsm_ctx.stats.state_transitions++;

    // Ações específicas na entrada de cada estado
    switch (new_state) {
        case MODE_DISPLAY:
            // Ensure the active effect (fsm_ctx.current_effect) and its params are loaded.
            // This is important if we exited a setup mode without saving, or switched from another mode.
            if (current_active_effect == NULL || current_active_effect->id != fsm_ctx.current_effect ||
                (current_active_effect->param_count > 0 && current_effect_params_runtime == NULL) ) {
                ESP_LOGI(TAG, "Transition to DISPLAY: Reloading params for current effect ID %d.", fsm_ctx.current_effect);
                fsm_load_effect_params(fsm_ctx.current_effect); // This also sets current_active_effect
            }
            fsm_perform_visual_feedback(FEEDBACK_ENTERING_DISPLAY_MODE);
            fsm_update_led_display();
            break;
            
        case MODE_EFFECT_SELECT:
            fsm_perform_visual_feedback(FEEDBACK_ENTERING_EFFECT_SELECT);
            // On entering effect selection, fsm_ctx.current_effect is the starting point.
            // The encoder will change fsm_ctx.current_effect and call fsm_load_effect_params.
            // We need to ensure the initial preview is correct.
            if (num_defined_effects > 0) {
                // Ensure params for the *current* effect (before selection changes) are loaded for preview
                if (current_active_effect == NULL || current_active_effect->id != fsm_ctx.current_effect ||
                    (current_active_effect->param_count > 0 && current_effect_params_runtime == NULL) ) {
                     ESP_LOGI(TAG, "Transition to EFFECT_SELECT: Ensuring params for initial effect ID %d are loaded for preview.", fsm_ctx.current_effect);
                     fsm_load_effect_params(fsm_ctx.current_effect);
                }
                fsm_update_led_display();
            } else {
                 ESP_LOGW(TAG, "Transition to EFFECT_SELECT: No effects defined. Display will be blank or off.");
                 led_controller_clear();
                 led_controller_refresh();
            }
            break;
            
        case MODE_EFFECT_SETUP:
            fsm_ctx.setup_effect_index = fsm_ctx.current_effect;
            ESP_LOGI(TAG, "Transition to EFFECT_SETUP for effect ID %d.", fsm_ctx.setup_effect_index);

            if (current_active_effect == NULL || current_active_effect->id != fsm_ctx.setup_effect_index ||
                (current_active_effect->param_count > 0 && current_effect_params_runtime == NULL) ) {
                ESP_LOGI(TAG, "Loading params for effect ID %d to be set up.", fsm_ctx.setup_effect_index);
                fsm_load_effect_params(fsm_ctx.setup_effect_index); // This sets current_active_effect
            } else {
                ESP_LOGI(TAG, "Params for effect %s (ID %d) already loaded for setup.", current_active_effect->name, current_active_effect->id);
            }

            // Verify that the effect to be set up is now active and has params if expected
            if (current_active_effect != NULL && current_active_effect->id == fsm_ctx.setup_effect_index) {
                 if (current_active_effect->param_count == 0) {
                    ESP_LOGW(TAG, "Effect %s (ID %d) has no parameters. Cannot enter setup. Reverting to DISPLAY.", current_active_effect->name, current_active_effect->id);
                    fsm_ctx.current_state = fsm_ctx.previous_state; // Revert state to prevent loop
                    fsm_transition_to_state(MODE_DISPLAY); // Go back to display mode
                    return ESP_FAIL; // Indicate failed transition
                 }
                 fsm_ctx.setup_param_index = 0;
                 fsm_ctx.setup_has_changes = false;
                 fsm_perform_visual_feedback(FEEDBACK_ENTERING_EFFECT_SETUP);
                 ESP_LOGI(TAG, "Starting setup for effect: %s. Editing param: %s (idx 0)",
                          current_active_effect->name,
                          (current_effect_params_runtime) ? current_effect_params_runtime[0].name : "N/A");
            } else {
                ESP_LOGE(TAG, "Failed to enter setup: Effect ID %d could not be loaded. Reverting to DISPLAY.", fsm_ctx.setup_effect_index);
                 fsm_ctx.current_state = fsm_ctx.previous_state;
                 fsm_transition_to_state(MODE_DISPLAY);
                 return ESP_FAIL;
            }
            // No fsm_update_led_display() here initially; changes are shown by encoder.
            // However, it might be good to show the current effect state clearly.
            fsm_update_led_display();
            break;
            
        case MODE_SYSTEM_SETUP:
            fsm_perform_visual_feedback(FEEDBACK_ENTERING_SYSTEM_SETUP);
            memcpy(system_params_temp_copy, system_params_array, sizeof(system_params_array)); // Load current settings into temp copy
            fsm_ctx.system_setup_param_index = 0;
            fsm_ctx.system_setup_has_changes = false;
            ESP_LOGI(TAG, "Entering System Setup. Editing: %s (Value: %" PRId32 ")",
                     system_params_temp_copy[fsm_ctx.system_setup_param_index].name,
                     system_params_temp_copy[fsm_ctx.system_setup_param_index].value);
            // Display for system setup parameters is primarily via log for now.
            // Optionally, could clear LEDs or show a specific pattern for system setup mode.
            // led_controller_clear();
            // led_controller_refresh();
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown state in transition: %d", new_state);
            return ESP_ERR_INVALID_ARG;
    }

    return ESP_OK;
}

// Verifica timeout de modo (volta para DISPLAY se ficar muito tempo em outros estados)
static void fsm_check_mode_timeout(uint32_t current_time) {
    if (fsm_ctx.current_state == MODE_DISPLAY) {
        return;  // Não há timeout no modo display
    }

    uint32_t time_in_state = current_time - fsm_ctx.state_entry_time;
    if (time_in_state > fsm_ctx.config.mode_timeout_ms) {
        ESP_LOGI(TAG, "Mode timeout in state %d, returning to display", fsm_ctx.current_state);
        fsm_transition_to_state(MODE_DISPLAY);
    }
}

// Atualiza display dos LEDs baseado no estado atual
static void fsm_update_led_display(void) {
    if (!fsm_ctx.led_strip_on) {
        led_controller_clear(); // Clears buffer
        led_controller_refresh(); // Sends cleared buffer to strip
        ESP_LOGD(TAG, "LEDs turned OFF. Cleared and refreshed.");
        return;
    }

    if (current_active_effect == NULL || current_effect_params_runtime == NULL) {
        if (num_defined_effects > 0 && current_active_effect == NULL) {
             // Attempt to load the default effect if none is active (e.g. after malloc fail)
            ESP_LOGW(TAG, "No active effect or params, attempting to load default effect ID %d", fsm_ctx.current_effect);
            fsm_load_effect_params(fsm_ctx.current_effect);
            // If still null, then clear the strip as a fallback
            if (current_active_effect == NULL || (current_active_effect->param_count > 0 && current_effect_params_runtime == NULL)) {
                ESP_LOGE(TAG, "Failed to load default effect or its params. Clearing strip.");
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
        // If current_active_effect is not null, but params are (for an effect with params), it's an issue.
        if (current_active_effect != NULL && current_active_effect->param_count > 0 && current_effect_params_runtime == NULL) {
            ESP_LOGE(TAG, "Effect %s (ID %d) has params but runtime_params is NULL. Clearing strip.", current_active_effect->name, current_active_effect->id);
            led_controller_clear();
            led_controller_refresh();
            return;
        }
        // If effect has no params, current_effect_params_runtime might be legitimately NULL.
    }
    
    // Apply global brightness - this is usually handled by led_controller_set_brightness
    // and subsequent led_controller_set_pixel_hsv will be scaled by it.
    // So, no explicit fsm_ctx.global_brightness application per pixel here.
    // led_controller_set_brightness(fsm_ctx.global_brightness); // Ensure it's up-to-date

    ESP_LOGD(TAG, "Updating LED display - Effect: %s (ID: %d), Brightness: %d, Power: %s",
             current_active_effect ? current_active_effect->name : "None",
             fsm_ctx.current_effect, fsm_ctx.global_brightness,
             fsm_ctx.led_strip_on ? "ON" : "OFF");

    switch (current_active_effect->id) {
        case 0: // Static Color
            if (current_active_effect->param_count >= 2 && current_effect_params_runtime != NULL) {
                uint16_t hue = current_effect_params_runtime[0].value; // Hue
                uint8_t sat = current_effect_params_runtime[1].value;  // Saturation
                // uint8_t val = 255; // Max value, global brightness will scale this
                // If effect has a 'Value' or 'Brightness' param itself:
                // uint8_t val = (current_active_effect->param_count > 2) ? current_effect_params_runtime[2].value : 255;

                for (uint32_t i = 0; i < LED_STRIP_NUM_LEDS; i++) {
                    // V in HSV is set to 255 (max) so global_brightness effectively controls perceived V.
                    led_controller_set_pixel_hsv(i, hue, sat, 255);
                }
            } else {
                ESP_LOGE(TAG, "Static Color effect (ID 0) misconfigured or params not loaded.");
                // Fallback: set LEDs to a default color like white or off
                for (uint32_t i = 0; i < LED_STRIP_NUM_LEDS; i++) {
                    led_controller_set_pixel_hsv(i, 0, 0, 100); // Dim white
                }
            }
            break;

        case 1: // Candle
            // Placeholder for candle effect rendering
            // This would involve more complex logic, potentially calling a function
            // from led_effects.c or a dedicated rendering function.
            // For now, set a placeholder color, e.g., warm yellow/orange from its default params
            if (current_active_effect->param_count >= 1 && current_effect_params_runtime != NULL) {
                 uint16_t base_hue = current_effect_params_runtime[0].value; // Base Hue
                // uint8_t flicker_speed = current_effect_params_runtime[1].value; // Flicker Speed (not used in static render)
                // uint8_t intensity = current_effect_params_runtime[2].value; // Intensity (not used in static render)
                for (uint32_t i = 0; i < LED_STRIP_NUM_LEDS; i++) {
                    // Basic static representation of the candle's base hue
                    led_controller_set_pixel_hsv(i, base_hue, 220, 255); // High saturation, max value
                }
            } else {
                 ESP_LOGE(TAG, "Candle effect (ID 1) misconfigured or params not loaded.");
                 for (uint32_t i = 0; i < LED_STRIP_NUM_LEDS; i++) {
                    led_controller_set_pixel_hsv(i, 30, 200, 255); // Default warm orange
                }
            }
            break;

        // Add other effects here by their ID
        default:
            ESP_LOGW(TAG, "Effect ID %d not implemented. Turning LEDs off.", current_active_effect->id);
            led_controller_clear();
            break;
    }
    led_controller_refresh(); // Send data to the strip
}

// Obtém timestamp atual em millisegundos
static uint32_t fsm_get_current_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

// Helper function to load parameters for a given effect ID
static void fsm_load_effect_params(uint8_t effect_id) {
    const led_effect_t* effect = led_effects_get_by_id(effect_id);
    if (effect == NULL) {
        ESP_LOGE(TAG, "Effect ID %d not found.", effect_id);
        // Keep current_active_effect and params as they are, or clear them?
        // For now, do nothing to prevent crashing if an invalid ID is somehow passed.
        // A better approach might be to load a default "error" effect or clear.
        return;
    }

    // Free existing runtime parameters if any
    if (current_effect_params_runtime != NULL) {
        free(current_effect_params_runtime);
        current_effect_params_runtime = NULL;
    }

    current_active_effect = effect;
    // fsm_ctx.current_effect = effect_id; // This is now set by the caller logic correctly

    if (effect->param_count > 0) {
        current_effect_params_runtime = malloc(sizeof(effect_param_t) * effect->param_count);
        if (current_effect_params_runtime) {
            memcpy(current_effect_params_runtime, effect->params, sizeof(effect_param_t) * effect->param_count);
            ESP_LOGI(TAG, "Loaded %d parameters for effect: %s (ID: %d)", effect->param_count, effect->name, effect_id);
        } else {
            ESP_LOGE(TAG, "Failed to allocate memory for runtime parameters of effect %s", effect->name);
            current_active_effect = NULL; // Cannot proceed with this effect if params cannot be loaded
        }
    } else {
        ESP_LOGI(TAG, "Effect %s (ID: %d) has no parameters.", effect->name, effect_id);
    }
}


// --- Visual Feedback Implementation (Simplified to Logs) ---
static void fsm_perform_visual_feedback(fsm_feedback_event_t feedback_type) {
    // For now, this function just logs the feedback event.
    // Actual LED blinking/animation would require more complex, non-blocking implementation,
    // potentially integrated with led_controller or a dedicated animation manager.
    const char* event_name = "UNKNOWN";
    switch (feedback_type) {
        case FEEDBACK_ENTERING_DISPLAY_MODE: event_name = "ENTERING_DISPLAY_MODE"; break;
        case FEEDBACK_ENTERING_EFFECT_SELECT: event_name = "ENTERING_EFFECT_SELECT"; break;
        case FEEDBACK_ENTERING_EFFECT_SETUP: event_name = "ENTERING_EFFECT_SETUP"; break;
        case FEEDBACK_ENTERING_SYSTEM_SETUP: event_name = "ENTERING_SYSTEM_SETUP"; break;
        case FEEDBACK_SAVED_PARAM: event_name = "SAVED_PARAM"; break;
        case FEEDBACK_SAVED_EFFECT_SETTINGS: event_name = "SAVED_EFFECT_SETTINGS"; break;
        case FEEDBACK_CANCEL_SETUP: event_name = "CANCEL_SETUP"; break;
        case FEEDBACK_EFFECT_SELECTED: event_name = "EFFECT_SELECTED"; break;
        case FEEDBACK_REVERT_EFFECT_SELECTION: event_name = "REVERT_EFFECT_SELECTION"; break;
        case FEEDBACK_POWER_ON: event_name = "POWER_ON"; break;
        case FEEDBACK_POWER_OFF: event_name = "POWER_OFF"; break;
        case FEEDBACK_SYSTEM_PARAM_SELECTED: event_name = "SYSTEM_PARAM_SELECTED"; break;
        case FEEDBACK_SYSTEM_SETTINGS_SAVED: event_name = "SYSTEM_SETTINGS_SAVED"; break;
        case FEEDBACK_SYSTEM_SETUP_CANCELLED: event_name = "SYSTEM_SETUP_CANCELLED"; break;
        case FEEDBACK_EXIT_SYSTEM_SETUP: event_name = "EXIT_SYSTEM_SETUP"; break;
        default: ESP_LOGW(TAG, "Unknown visual feedback type: %d", feedback_type); return;
    }
    ESP_LOGI(TAG, "Visual Feedback Event: %s", event_name);

    // Example of how real feedback might be triggered (conceptually):
    // if (led_controller_is_available()) { // Check if LED controller is working
    //     led_controller_play_feedback_animation(feedback_type); // Imaginary non-blocking function
    // }
}

// --- Apply System Settings ---
static void fsm_apply_system_settings(void) {
    ESP_LOGI(TAG, "Applying system settings...");

    // Parameter 0: Mode Timeout
    // Apply from system_params_array as it holds the "saved" (committed) values.
    if (system_params_array[0].value * 1000 != fsm_ctx.config.mode_timeout_ms) {
        fsm_ctx.config.mode_timeout_ms = system_params_array[0].value * 1000;
        ESP_LOGI(TAG, "Mode timeout updated to %" PRIu32 " ms", fsm_ctx.config.mode_timeout_ms);
    }

    // Parameter 1: Default Brightness
    uint8_t new_brightness = (uint8_t)system_params_array[1].value;
    if (new_brightness != fsm_ctx.global_brightness) {
         fsm_ctx.global_brightness = new_brightness;
         led_controller_set_brightness(fsm_ctx.global_brightness);
         ESP_LOGI(TAG, "Global brightness updated to %d via system settings", fsm_ctx.global_brightness);
         fsm_update_led_display(); // Update display to reflect brightness change
    }

    // TODO: Save system_params_array to NVS here.
    ESP_LOGI(TAG, "System settings applied from system_params_array. (NVS save is TODO)");
    fsm_ctx.system_setup_has_changes = false; // Reset flag as changes are now "applied" / committed
}
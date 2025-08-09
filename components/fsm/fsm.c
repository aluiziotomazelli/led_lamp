#include "fsm.h"
#include "esp_timer.h"
#include "led_controller.h"  // Para controle dos LEDs
#include "project_config.h"
#include <string.h>
#include <inttypes.h>  // Necessário para as macros PRIu32, PRId32, etc.

static const char *TAG = "FSM";

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
    uint8_t current_effect;             ///< Efeito atualmente selecionado
    uint8_t global_brightness;          ///< Brilho global (0-254)
    bool led_strip_on;                  ///< Estado da fita LED
    
    // Setup state
    uint8_t setup_param_index;          ///< Parâmetro sendo editado no setup
    uint8_t setup_effect_index;         ///< Efeito sendo editado no setup
    bool setup_has_changes;             ///< Flag de mudanças não salvas
    
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
    fsm_ctx.current_effect = 0;
    fsm_ctx.global_brightness = 128;  // 50% de brilho inicial
    fsm_ctx.led_strip_on = true;
    fsm_ctx.setup_param_index = 0;
    fsm_ctx.setup_effect_index = 0;
    fsm_ctx.setup_has_changes = false;
    
    // Limpa estatísticas
    memset(&fsm_ctx.stats, 0, sizeof(fsm_stats_t));
    fsm_ctx.stats.current_state = fsm_ctx.current_state;

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

    // Inicializa display dos LEDs
    fsm_update_led_display();

    ESP_LOGI(TAG, "FSM initialized successfully in state %" PRIu8, fsm_ctx.current_state);
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
    
    ESP_LOGW(TAG, "Forcing state transition from %" PRIu8 " to %" PRIu8, fsm_ctx.current_state, new_state);
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
            ESP_LOGW(TAG, "Unknown event source: %" PRIu8, event->source);
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

    ESP_LOGD(TAG, "Button event: %" PRIu8 " in state %" PRIu8, button_event->type, fsm_ctx.current_state);

    switch (fsm_ctx.current_state) {
        case MODE_DISPLAY:
            switch (button_event->type) {
                case BUTTON_CLICK:
                    // Liga/desliga a fita LED
                    fsm_ctx.led_strip_on = !fsm_ctx.led_strip_on;
                    fsm_update_led_display();
                    ESP_LOGI(TAG, "LED strip %s", fsm_ctx.led_strip_on ? "ON" : "OFF");
                    break;
                    
                case BUTTON_DOUBLE_CLICK:
                    // Entra no modo seleção de efeito
                    fsm_transition_to_state(MODE_EFFECT_SELECT);
                    break;
                    
                case BUTTON_LONG_CLICK:
                    // Entra no setup do efeito atual
                    fsm_ctx.setup_effect_index = fsm_ctx.current_effect;
                    fsm_ctx.setup_param_index = 0;
                    fsm_ctx.setup_has_changes = false;
                    fsm_transition_to_state(MODE_EFFECT_SETUP);
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
                    // Seleciona o efeito atual e volta para exibição
                    fsm_transition_to_state(MODE_DISPLAY);
                    fsm_update_led_display();
                    ESP_LOGI(TAG, "Effect %" PRIu8 " selected", fsm_ctx.current_effect);
                    break;
                    
                case BUTTON_LONG_CLICK:
                    // Cancela seleção e volta para exibição
                    fsm_transition_to_state(MODE_DISPLAY);
                    break;
                    
                default:
                    break;
            }
            break;

        case MODE_EFFECT_SETUP:
            switch (button_event->type) {
                case BUTTON_CLICK:
                    // Salva parâmetro atual e vai para o próximo
                    fsm_ctx.setup_param_index++;
                    fsm_ctx.setup_has_changes = true;
                    // TODO: Verificar se ainda há parâmetros para configurar
                    // Se não há mais, salva e volta para exibição
                    ESP_LOGI(TAG, "Setup param %" PRIu8 " saved, moving to next", fsm_ctx.setup_param_index - 1);
                    break;
                    
                case BUTTON_LONG_CLICK:
                    // Sai do setup e volta para exibição
                    if (fsm_ctx.setup_has_changes) {
                        // TODO: Salvar configurações na flash
                        ESP_LOGI(TAG, "Setup changes saved");
                    }
                    fsm_transition_to_state(MODE_DISPLAY);
                    break;
                    
                default:
                    break;
            }
            break;

        case MODE_SYSTEM_SETUP:
            switch (button_event->type) {
                case BUTTON_CLICK:
                    // Navega pelos parâmetros do sistema
                    ESP_LOGI(TAG, "System setup navigation");
                    break;
                    
                case BUTTON_LONG_CLICK:
                    // Sai do setup do sistema
                    fsm_transition_to_state(MODE_DISPLAY);
                    break;
                    
                default:
                    break;
            }
            break;

        default:
            ESP_LOGW(TAG, "Unknown state in button processing: %" PRIu8, fsm_ctx.current_state);
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

    ESP_LOGD(TAG, "Encoder event: %" PRId32 " steps in state %" PRIu8, encoder_event->steps, fsm_ctx.current_state);

    switch (fsm_ctx.current_state) {
        case MODE_DISPLAY:
            // Ajusta brilho global
            if (encoder_event->steps > 0) {
                fsm_ctx.global_brightness = (fsm_ctx.global_brightness >= 254 - encoder_event->steps) ? 
                                           254 : fsm_ctx.global_brightness + encoder_event->steps;
            } else {
                fsm_ctx.global_brightness = (fsm_ctx.global_brightness <= -encoder_event->steps) ? 
                                           0 : fsm_ctx.global_brightness + encoder_event->steps;
            }
            fsm_update_led_display();
            ESP_LOGD(TAG, "Brightness adjusted to %" PRIu8, fsm_ctx.global_brightness);
            break;

        case MODE_EFFECT_SELECT:
            // Navega entre efeitos
            if (encoder_event->steps > 0) {
                fsm_ctx.current_effect++;
                // TODO: Verificar limite máximo de efeitos
            } else if (encoder_event->steps < 0 && fsm_ctx.current_effect > 0) {
                fsm_ctx.current_effect--;
            }
            fsm_update_led_display();  // Mostra preview do efeito
            ESP_LOGD(TAG, "Effect selection: %" PRIu8, fsm_ctx.current_effect);
            break;

        case MODE_EFFECT_SETUP:
            // Ajusta parâmetro do efeito atual
            // TODO: Implementar ajuste de parâmetros específicos do efeito
            ESP_LOGD(TAG, "Adjusting effect parameter %" PRIu8 " by %" PRId32 " steps", 
                     fsm_ctx.setup_param_index, encoder_event->steps);
            break;

        case MODE_SYSTEM_SETUP:
            // Ajusta parâmetros do sistema
            ESP_LOGD(TAG, "Adjusting system parameter %" PRId32 " steps", encoder_event->steps);
            break;

        default:
            ESP_LOGW(TAG, "Unknown state in encoder processing: %" PRIu8, fsm_ctx.current_state);
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

    ESP_LOGD(TAG, "ESP-NOW event: %" PRIu16 " bytes from %02x:%02x:%02x:%02x:%02x:%02x",
             espnow_event->data_len,
             espnow_event->mac_addr[0], espnow_event->mac_addr[1], espnow_event->mac_addr[2],
             espnow_event->mac_addr[3], espnow_event->mac_addr[4], espnow_event->mac_addr[5]);

    // TODO: Implementar processamento de comandos ESP-NOW
    // Pode incluir: controle remoto, sincronização entre dispositivos, etc.

    return ESP_OK;
}

// Faz transição entre estados
static esp_err_t fsm_transition_to_state(fsm_state_t new_state) {
    if (new_state == fsm_ctx.current_state) {
        return ESP_OK;  // Já está no estado desejado
    }

    ESP_LOGI(TAG, "State transition: %" PRIu8 " -> %" PRIu8, fsm_ctx.current_state, new_state);
    
    fsm_ctx.previous_state = fsm_ctx.current_state;
    fsm_ctx.current_state = new_state;
    fsm_ctx.state_entry_time = fsm_get_current_time_ms();
    fsm_ctx.stats.state_transitions++;

    // Ações específicas na entrada de cada estado
    switch (new_state) {
        case MODE_DISPLAY:
            // Volta ao modo normal de exibição
            fsm_update_led_display();
            break;
            
        case MODE_EFFECT_SELECT:
            // Mostra preview do efeito atual
            fsm_update_led_display();
            break;
            
        case MODE_EFFECT_SETUP:
            // Inicia setup do efeito
            ESP_LOGI(TAG, "Starting effect setup for effect %" PRIu8, fsm_ctx.setup_effect_index);
            break;
            
        case MODE_SYSTEM_SETUP:
            // Inicia setup do sistema
            ESP_LOGI(TAG, "Starting system setup");
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown state in transition: %" PRIu8, new_state);
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
        ESP_LOGI(TAG, "Mode timeout in state %" PRIu8 ", returning to display", fsm_ctx.current_state);
        fsm_transition_to_state(MODE_DISPLAY);
    }
}

// Atualiza display dos LEDs baseado no estado atual
static void fsm_update_led_display(void) {
    // TODO: Implementar chamadas para o LED controller
    // led_controller_set_effect(fsm_ctx.current_effect);
    // led_controller_set_brightness(fsm_ctx.global_brightness);
    // led_controller_set_power(fsm_ctx.led_strip_on);
    
    ESP_LOGD(TAG, "LED display updated - Effect: %" PRIu8 ", Brightness: %" PRIu8 ", Power: %s",
             fsm_ctx.current_effect, fsm_ctx.global_brightness,
             fsm_ctx.led_strip_on ? "ON" : "OFF");
}

// Obtém timestamp atual em millisegundos
static uint32_t fsm_get_current_time_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}
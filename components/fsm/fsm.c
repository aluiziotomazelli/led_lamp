#include "fsm.h"
#include <freertos/task.h>
#include <math.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
static const char *TAG = "FSM";
#include "esp_log.h"


// --- Variáveis estáticas (estado interno) ---
static fsm_state_t current_state;
static uint8_t current_brightness = 128; // Valor padrão
static uint8_t current_effect_id = 0;    // ID do efeito atual
static uint8_t last_effect_id;           // Para efeito de "cancelar"
static uint8_t current_param_index;      // Índice do parâmetro atual no setup


// --- Protótipos de Funções Locais ---
static void fsm_task(void *pvParameters);
static void handle_off_state(const integrated_event_t *event);
static void handle_display_state(const integrated_event_t *event);
static void handle_effect_change_state(const integrated_event_t *event);
static void handle_effect_setup_state(const integrated_event_t *event);
static void handle_button_in_display(button_event_t btn);
static void adjust_brightness(int16_t steps);
static void handle_button_in_effect_change(button_event_t btn);
static void change_effect_preview(int16_t steps);
static void handle_button_in_effect_setup(button_event_t btn);
static void adjust_effect_param(int16_t steps);
static void send_action(fsm_action_type_t type, const void* data);
static uint8_t get_next_effect(uint8_t current_effect, int8_t direction);
static bool is_last_parameter(uint8_t param_index);

//Filas estáticas dentro da task
static QueueHandle_t input_queue;
static QueueHandle_t output_queue;

// --- Implementação das Funções Públicas ---
void fsm_init(fsm_queues_t queues) {
	input_queue = queues.input_queue;
    output_queue = queues.output_queue;

    xTaskCreate(fsm_task, "fsm_task", 4096, NULL, tskIDLE_PRIORITY+1, NULL);
}



// --- Tarefa Principal da FSM ---
static void fsm_task(void *pvParameters) {
	(void)pvParameters;
    integrated_event_t event;

    // Estado inicial
    current_state = MODE_OFF;
    // Carregar estado inicial da NVS
    send_action(ACTION_LOAD_CONFIG, NULL);

    while (1) {
        if (xQueueReceive(input_queue, &event, portMAX_DELAY)) {
            switch (current_state) {
                case MODE_OFF:
                    handle_off_state(&event);
                    break;
                case MODE_DISPLAY:
                    handle_display_state(&event);
                    break;
                case MODE_EFFECT_CHANGE:
                    handle_effect_change_state(&event);
                    break;
                case MODE_EFFECT_SETUP:
                    handle_effect_setup_state(&event);
                    break;
                case MODE_SYSTEM_SETUP:
                    // TODO: Implementar
                    break;
            }
        }
    }
}

// --- Manipuladores de Estado ---
static void handle_off_state(const integrated_event_t *event) {
    if (event->source == EVENT_SOURCE_BUTTON && 
        event->data.button.type == BUTTON_CLICK) {
        
        current_state = MODE_DISPLAY;
        send_action(ACTION_TURN_ON_LAMP, NULL);
        ESP_LOGD(TAG, "State: %d", current_state);
        send_action(ACTION_FEEDBACK_SHORT, NULL);
    }
}

static void handle_display_state(const integrated_event_t *event) {
    switch (event->source) {
        case EVENT_SOURCE_BUTTON:
            handle_button_in_display(event->data.button);
            break;
        case EVENT_SOURCE_ENCODER:
            adjust_brightness(event->data.encoder.steps);
            break;
        default:
            break;
    }
}

static void handle_button_in_display(button_event_t btn) {
    switch (btn.type) {
        case BUTTON_CLICK:
            send_action(ACTION_SAVE_CONFIG, NULL);
            send_action(ACTION_TURN_OFF_LAMP, NULL);
            current_state = MODE_OFF;
            send_action(ACTION_FEEDBACK_SHORT, NULL);
	        ESP_LOGD(TAG, "Desligando");
            break;
            
        case BUTTON_LONG_CLICK:
            last_effect_id = current_effect_id; // Salva o efeito atual para possível cancelamento
            current_state = MODE_EFFECT_CHANGE;
            send_action(ACTION_FEEDBACK_SHORT, NULL);
	        ESP_LOGD(TAG, "Troca efeito");
            break;

        case BUTTON_DOUBLE_CLICK:
            current_state = MODE_EFFECT_SETUP;
            current_param_index = 0;
            send_action(ACTION_SELECT_PARAMETER, &current_param_index); // Informa o primeiro parâmetro
            send_action(ACTION_FEEDBACK_LONG, NULL);
	        ESP_LOGD(TAG, "Setup efeito");
            break;
        default:
            break;
    }
}

static void adjust_brightness(int16_t steps) {
    int16_t new_brightness = current_brightness + steps;
    new_brightness = (new_brightness < 0) ? 0 : new_brightness;
    new_brightness = (new_brightness > 254) ? 254 : new_brightness;
    
    if (current_brightness != (uint8_t)new_brightness) {
        current_brightness = (uint8_t)new_brightness;
        send_action(ACTION_ADJUST_BRIGHTNESS, &current_brightness);
    }
}

static void handle_effect_change_state(const integrated_event_t *event) {
    switch (event->source) {
        case EVENT_SOURCE_BUTTON:
            handle_button_in_effect_change(event->data.button);
            break;
        case EVENT_SOURCE_ENCODER:
            change_effect_preview(event->data.encoder.steps);
            break;
        default:
            break;
    }
}

static void handle_button_in_effect_change(button_event_t btn) {
    switch (btn.type) {
        case BUTTON_CLICK:
            send_action(ACTION_SELECT_EFFECT, &current_effect_id);
            current_state = MODE_DISPLAY;
            send_action(ACTION_FEEDBACK_SHORT, NULL);
	        ESP_LOGD(TAG, "Confirmando efeito");
            break;
            
        case BUTTON_DOUBLE_CLICK:
            current_effect_id = last_effect_id;
            send_action(ACTION_SELECT_EFFECT, &last_effect_id);
            current_state = MODE_DISPLAY;
            send_action(ACTION_FEEDBACK_SHORT, NULL);
	        ESP_LOGD(TAG, "Cancelando e voltando efeito anterior");
            break;
            
        default:
            break;
    }
}

static void change_effect_preview(int16_t steps) {
    static int16_t accumulator = 0;
    const uint8_t threshold = 4; // Passos necessários para mudar de efeito
    
    accumulator += steps;
    if (abs(accumulator) >= threshold) {
        int8_t direction = (accumulator > 0) ? 1 : -1;
        current_effect_id = get_next_effect(current_effect_id, direction);
        send_action(ACTION_SHOW_EFFECT_PREVIEW, &current_effect_id);
        accumulator = 0;
    }
}

static void handle_effect_setup_state(const integrated_event_t *event) {
    switch (event->source) {
        case EVENT_SOURCE_BUTTON:
            handle_button_in_effect_setup(event->data.button);
            break;
        case EVENT_SOURCE_ENCODER:
            adjust_effect_param(event->data.encoder.steps);
            break;
        default:
            break;
    }
}

static void handle_button_in_effect_setup(button_event_t btn) {
    switch (btn.type) {
        case BUTTON_CLICK:
            if (is_last_parameter(current_param_index)) {
                current_state = MODE_DISPLAY;
                send_action(ACTION_EXIT_SETUP, NULL);
		        ESP_LOGD(TAG, "Ultimo parametro, saindo setup");
            } else {
                current_param_index++;
                send_action(ACTION_SELECT_PARAMETER, &current_param_index);
		        ESP_LOGD(TAG, "Proximo parâmetro");
            }
            send_action(ACTION_FEEDBACK_SHORT, NULL);
            break;
            
        case BUTTON_LONG_CLICK:
            current_state = MODE_DISPLAY;
            send_action(ACTION_EXIT_SETUP, NULL);
            send_action(ACTION_FEEDBACK_LONG, NULL);
		    ESP_LOGD(TAG, "Saindo do Setup");
            break;
            
        case BUTTON_DOUBLE_CLICK:
            if (current_param_index > 0) {
                current_param_index--;
                send_action(ACTION_SELECT_PARAMETER, &current_param_index);
                send_action(ACTION_FEEDBACK_SHORT, NULL);
			    ESP_LOGD(TAG, "Cancelando Setup");
            }
            break;
            
        default:
            break;
    }
}

static void adjust_effect_param(int16_t steps) {
    effect_param_adjust_t adjustment = {
        .param_id = current_param_index,
        .delta = steps
    };
    send_action(ACTION_ADJUST_EFFECT_PARAM, &adjustment);
}

// --- Funções Auxiliares ---
static void send_action(fsm_action_type_t type, const void* data) {
    fsm_output_action_t action = { .type = type };

    if (data != NULL) {
        switch (type) {
            case ACTION_ADJUST_BRIGHTNESS:
                action.data.brightness = *(const uint8_t*)data;
                break;
            case ACTION_SELECT_EFFECT:
            case ACTION_SHOW_EFFECT_PREVIEW:
                action.data.effect_id = *(const uint8_t*)data;
                break;
            case ACTION_ADJUST_EFFECT_PARAM:
                action.data.effect_param_adj = *(const effect_param_adjust_t*)data;
                break;
            case ACTION_SELECT_PARAMETER:
                action.data.param_index = *(const uint8_t*)data;
                break;
            default:
                // Outras ações podem não ter dados ou não são tratadas aqui
                break;
        }
    }

    if (output_queue != NULL) {
        xQueueSend(output_queue, &action, portMAX_DELAY);
    }
}

static uint8_t get_next_effect(uint8_t current_effect, int8_t direction) {
    // Implementação real depende da lista de efeitos disponíveis
    const uint8_t effect_count = 10; // Número total de efeitos (deve ser obtido de um gerenciador)
    int16_t new_effect = (int16_t)current_effect + direction;
    
    // Trata rotação circular
    if (new_effect < 0) {
        new_effect = effect_count - 1;
    } else if (new_effect >= effect_count) {
        new_effect = 0;
    }
    
    return (uint8_t)new_effect;
}

static bool is_last_parameter(uint8_t param_index) {
    // Implementação real depende do efeito atual
    // Exemplo: supondo que todos os efeitos tenham 3 parâmetros
    return (param_index >= 2);
}
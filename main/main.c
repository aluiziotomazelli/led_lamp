#include <stdio.h>
#include <stdlib.h> // For malloc and free
#include "freertos/FreeRTOS.h"
#include "esp_log_level.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "button.h"
#include "soc/gpio_num.h"
#include "system_events.h"
#include "encoder.h"
#include "input_event_manager.h" // For integrated events

static const char *TAG = "MAIN";
static const char *TAG_BUTTON_HANDLER = "BTN_HANDLER";
static const char *TAG_ENCODER_HANDLER = "ENC_HANDLER";
// TAG_INTEGRATED_HANDLER is defined later
static const char *TAG_EVENT_HANDLERS = "EVT_HANDLERS";
static const char *TAG_CENTRAL_PROCESSOR = "CENTRAL_PROC";

QueueHandle_t button_app_queue = NULL;
QueueHandle_t encoder_event_queue = NULL;
QueueHandle_t integrated_queue = NULL;

static void app_encoder_event_handler_task(void *param) {
    encoder_event_t evt;
    ESP_LOGI(TAG_ENCODER_HANDLER, "Task de handler de eventos de encoder iniciada");

    while(1) {
        if (xQueueReceive(encoder_event_queue, &evt, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG_ENCODER_HANDLER, "Encoder event: steps %ld", evt.steps);
            // Here you could also map encoder_event_t to system_event_t if needed
            // For now, just logging raw encoder steps.
        }
    }
}

// Handler for button events
static void handle_button_events(QueueHandle_t button_q, QueueHandle_t integrated_q) {
    if (!button_q || !integrated_q) {
        ESP_LOGE(TAG_EVENT_HANDLERS, "Button handler: Invalid queue handle(s).");
        return;
    }
    button_event_t btn_event;
    if (xQueueReceive(button_q, &btn_event, pdMS_TO_TICKS(10)) == pdTRUE) { // Small timeout to prevent indefinite block if logic error
        ESP_LOGD(TAG_EVENT_HANDLERS, "Handling button event: Pin %d, Type %d", btn_event.pin, btn_event.type);
        integrated_event_t event_to_send;
        event_to_send.source = EVENT_SOURCE_BUTTON;
        event_to_send.data.button = btn_event;
        if (xQueueSend(integrated_q, &event_to_send, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG_EVENT_HANDLERS, "Failed to send integrated button event to queue.");
        } else {
            ESP_LOGD(TAG_EVENT_HANDLERS, "Integrated button event sent.");
        }
    } else {
        ESP_LOGW(TAG_EVENT_HANDLERS, "Failed to receive button event from queue after notification.");
    }
}

// Handler for encoder events
static void handle_encoder_events(QueueHandle_t encoder_q, QueueHandle_t integrated_q) {
    if (!encoder_q || !integrated_q) {
        ESP_LOGE(TAG_EVENT_HANDLERS, "Encoder handler: Invalid queue handle(s).");
        return;
    }
    encoder_event_t enc_event;
    if (xQueueReceive(encoder_q, &enc_event, pdMS_TO_TICKS(10)) == pdTRUE) {
        ESP_LOGD(TAG_EVENT_HANDLERS, "Handling encoder event: Steps %d", (int)enc_event.steps);
        integrated_event_t event_to_send;
        event_to_send.source = EVENT_SOURCE_ENCODER;
        event_to_send.data.encoder = enc_event;
        if (xQueueSend(integrated_q, &event_to_send, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG_EVENT_HANDLERS, "Failed to send integrated encoder event to queue.");
        } else {
            ESP_LOGD(TAG_EVENT_HANDLERS, "Integrated encoder event sent.");
        }
    } else {
        ESP_LOGW(TAG_EVENT_HANDLERS, "Failed to receive encoder event from queue after notification.");
    }
}

// Placeholder for ESP-NOW handler
// static void handle_espnow_events(QueueHandle_t espnow_q, QueueHandle_t integrated_q) { ... }

// Task parameters for central_event_processor_task
typedef struct {
    QueueHandle_t integrated_q_handle; // For output
    QueueHandle_t button_q_handle;     // For handle_button_events
    QueueHandle_t encoder_q_handle;    // For handle_encoder_events
    // QueueHandle_t espnow_q_handle;  // For handle_espnow_events (future)
} central_processor_params_t;

static void central_event_processor_task(void *pvParameters) {
    central_processor_params_t *params = (central_processor_params_t *)pvParameters;
    if (!params || !params->integrated_q_handle || !params->button_q_handle || !params->encoder_q_handle) {
        ESP_LOGE(TAG_CENTRAL_PROCESSOR, "Invalid parameters. Exiting task.");
        if(params) free(params); // Free if allocated
        vTaskDelete(NULL);
        return;
    }

    QueueHandle_t integrated_q = params->integrated_q_handle;
    QueueHandle_t button_q = params->button_q_handle;
    QueueHandle_t encoder_q = params->encoder_q_handle;
    // QueueHandle_t espnow_q = params->espnow_q_handle; // Future

    uint32_t notification_value;

    ESP_LOGI(TAG_CENTRAL_PROCESSOR, "Central event processor task started. Waiting for notifications.");

    while (1) {
        // Wait indefinitely for a notification
        if (xTaskNotifyWait(0x00,           /* Don't clear any bits on entry. */
                             CLEAR_FLAGS,    /* Clear all bits on exit. */
                             &notification_value, /* Stores the notification value. */
                             portMAX_DELAY) == pdTRUE) {

            ESP_LOGD(TAG_CENTRAL_PROCESSOR, "Notification received: 0x%lx", notification_value);

            if (notification_value & BUTTON_FLAG) {
                ESP_LOGI(TAG_CENTRAL_PROCESSOR, "Button flag received. Calling button handler.");
                handle_button_events(button_q, integrated_q);
            }
            if (notification_value & ENCODER_FLAG) {
                ESP_LOGI(TAG_CENTRAL_PROCESSOR, "Encoder flag received. Calling encoder handler.");
                handle_encoder_events(encoder_q, integrated_q);
            }
            // if (notification_value & ESPNOW_FLAG) {
            // ESP_LOGI(TAG_CENTRAL_PROCESSOR, "ESP-NOW flag received. Calling ESP-NOW handler.");
            // handle_espnow_events(espnow_q, integrated_q);
            // }

            // Check if multiple flags were set, though adapters send one at a time.
            // If they could send multiple, the handlers would be called sequentially here.
        }
        // else: xTaskNotifyWait timed out (not possible with portMAX_DELAY unless an error)
    }
}

static const char *TAG_INTEGRATED_HANDLER = "INT_EVT_HANDLER";

static void integrated_event_consumer_task(void *pvParameters) {
    QueueHandle_t queue = (QueueHandle_t)pvParameters;
    integrated_event_t event;

    ESP_LOGI(TAG_INTEGRATED_HANDLER, "Integrated event consumer task started.");

    while (1) {
        if (xQueueReceive(queue, &event, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG_INTEGRATED_HANDLER, "Received integrated event from source: %d", event.source);
            switch (event.source) {
                case EVENT_SOURCE_BUTTON:
                    ESP_LOGI(TAG_INTEGRATED_HANDLER, "Button Event: Pin: %d, Type: %d",
                             event.data.button.pin, event.data.button.type);
                    break;
                case EVENT_SOURCE_ENCODER:
                    ESP_LOGI(TAG_INTEGRATED_HANDLER, "Encoder Event: Steps: %d",
                             (int)event.data.encoder.steps);
                    break;
                case EVENT_SOURCE_ESPNOW:
                    // ESP_LOGI(TAG_INTEGRATED_HANDLER, "ESP-NOW Event: (data to be defined)");
                    // For now, just log that it was an ESP-NOW event if it occurs
                    ESP_LOGI(TAG_INTEGRATED_HANDLER, "ESP-NOW Event received (details TBD).");
                    break;
                default:
                    ESP_LOGW(TAG_INTEGRATED_HANDLER, "Unknown event source: %d", event.source);
                    break;
            }
        }
    }
}

static void app_button_event_handler_task(void *param) {
    button_event_t raw_event;
    ESP_LOGI(TAG_BUTTON_HANDLER, "Task de handler de eventos de botão iniciada");

    while (1) {
        if (xQueueReceive(button_app_queue, &raw_event, portMAX_DELAY)) {
            ESP_LOGI(TAG_BUTTON_HANDLER, "Raw button event: pin %d, type %d", raw_event.pin, raw_event.type);

            system_event_t sys_event;
            sys_event.data.button.button_id = raw_event.pin;

            switch (raw_event.type) {
                case BUTTON_CLICK:
                    sys_event.type = EVENT_BUTTON_CLICK;
                    break;
                case BUTTON_DOUBLE_CLICK:
                    sys_event.type = EVENT_BUTTON_DOUBLE_CLICK;
                    break;
                case BUTTON_LONG_CLICK:
                    sys_event.type = EVENT_BUTTON_LONG_CLICK;
                    break;
                case BUTTON_VERY_LONG_CLICK:
                    sys_event.type = EVENT_BUTTON_VERY_LONG_CLICK;
                    break;
                case BUTTON_TIMEOUT:
                    sys_event.type = EVENT_BUTTON_TIMEOUT;
                    break;
                case BUTTON_ERROR:
                    sys_event.type = EVENT_BUTTON_ERROR;
                    break;
                default:
                    sys_event.type = EVENT_NONE; // Ou lidar com evento desconhecido
            }

            if (sys_event.type != EVENT_NONE) {
                if (system_event_send(&sys_event)) {
                    ESP_LOGD(TAG_BUTTON_HANDLER, "Evento do sistema enviado: %d para o botão %d", sys_event.type, sys_event.data.button.button_id);
                } else {
                    ESP_LOGW(TAG_BUTTON_HANDLER, "Falha ao enviar evento do sistema para o botão %d", sys_event.data.button.button_id);
                }
            }
        }
    }
}

void system_event_task(void *param) {
    system_event_t evt;

    ESP_LOGI(TAG, "Task de eventos do sistema iniciada");

    while (1) {
        // Aguarda eventos do sistema (timeout 0 = espera infinita)
        if (system_event_receive(&evt, 0)) {
            switch (evt.type) {
                case EVENT_BUTTON_CLICK:
                    ESP_LOGI(TAG, ">> Clique simples - Botão %d", evt.data.button.button_id);
                    break;
                case EVENT_BUTTON_DOUBLE_CLICK:
                    ESP_LOGI(TAG, ">> Clique duplo - Botão %d", evt.data.button.button_id);
                    break;
                case EVENT_BUTTON_LONG_CLICK:
                    ESP_LOGI(TAG, ">> Clique longo - Botão %d", evt.data.button.button_id);
                    break;
                case EVENT_BUTTON_VERY_LONG_CLICK:
                    ESP_LOGI(TAG, ">> Clique muito longo - Botão %d", evt.data.button.button_id);
                    break;
                case EVENT_BUTTON_TIMEOUT:
                    ESP_LOGW(TAG, ">> Timeout de clique - Botão %d", evt.data.button.button_id);
                    break;
                case EVENT_BUTTON_ERROR: // Note: tem typo no enum (EVEENT)
                    ESP_LOGE(TAG, ">> Erro no botão %d", evt.data.button.button_id);
                    break;
                case EVENT_ENCODER_ROTATION:
                    ESP_LOGI(TAG, ">> Encoder rotacionado: %ld steps", evt.data.encoder.steps);
                    break;
                case EVENT_NONE:
                    ESP_LOGD(TAG, ">> Evento vazio recebido");
                    break;
                default:
                    ESP_LOGW(TAG, ">> Evento desconhecido: %d", evt.type);
                    break;
            }
        }
    }
}

void app_main(void) {
    TaskHandle_t central_processor_task_handle = NULL;
    // Configura nível de log para debug
    esp_log_level_set("*", ESP_LOG_DEBUG);
    esp_log_level_set("input_event_mgr", ESP_LOG_DEBUG); // Enable debug for the manager
    esp_log_level_set(TAG_INTEGRATED_HANDLER, ESP_LOG_INFO);
    
    ESP_LOGI(TAG, "=== TESTE DO SISTEMA DE EVENTOS ===");
    ESP_LOGI(TAG, "Inicializando sistema...");

    // IMPORTANTE: Inicializa o sistema de eventos ANTES de criar botões
    ESP_LOGI(TAG, "Inicializando sistema de eventos...");
    system_events_init();

    // Cria a fila para eventos brutos de botão
    ESP_LOGI(TAG, "Criando fila de eventos de botão...");
    button_app_queue = xQueueCreate(10, sizeof(button_event_t));
    if (button_app_queue == NULL) {
        ESP_LOGE(TAG, "FALHA ao criar fila de eventos de botão! Abortando...");
        return;
    }
    ESP_LOGI(TAG, "Fila de eventos de botão criada com sucesso.");

    // Cria botão no GPIO 23
    ESP_LOGI(TAG, "Criando botão no GPIO 23...");
    button_t *btn = button_create(GPIO_NUM_23, button_app_queue);
    if (!btn) {
        ESP_LOGE(TAG, "FALHA ao criar botão no GPIO 23! Abortando...");
        // Considere deletar a fila aqui se necessário, mas como é app_main, pode só retornar.
        return;
    }
    ESP_LOGI(TAG, "Botão no GPIO 23 criado com sucesso!");

    // Configura tempos personalizados (opcional)
    ESP_LOGI(TAG, "Configurando tempos do botão no GPIO 23...");
    button_set_click_times(btn, 200, 1000, 3000);
    ESP_LOGI(TAG, "Tempos configurados para GPIO 23.");

    // Cria a fila para eventos de encoder
    ESP_LOGI(TAG, "Criando fila de eventos de encoder...");
    encoder_event_queue = xQueueCreate(5, sizeof(encoder_event_t));
    if (encoder_event_queue == NULL) {
        ESP_LOGE(TAG, "FALHA ao criar fila de eventos de encoder! Abortando...");
        return;
    }
    ESP_LOGI(TAG, "Fila de eventos de encoder criada com sucesso.");

    ESP_LOGI(TAG, "Criando fila de eventos integrados...");
    integrated_queue = xQueueCreate(10, sizeof(integrated_event_t)); // Size 10, adjust if needed
    if (integrated_queue == NULL) {
        ESP_LOGE(TAG, "FALHA ao criar fila de eventos integrados! Abortando...");
        // Consider cleanup of other queues if this is critical path
        return;
    }
    ESP_LOGI(TAG, "Fila de eventos integrados criada com sucesso.");

    // Inicializa o componente Encoder
    ESP_LOGI(TAG, "Inicializando componente encoder...");
    encoder_handle_t my_encoder = NULL;
    encoder_config_t enc_config = {
        .pin_a = GPIO_NUM_17,
        .pin_b = GPIO_NUM_16,
        .half_step_mode = true,
        // .flip_direction = false, // This line is removed
        .acceleration_enabled = true,
        .accel_gap_ms = 50,
        .accel_max_multiplier = 5
    };
    my_encoder = encoder_create(&enc_config, encoder_event_queue);

    if (my_encoder == NULL) {
        ESP_LOGE(TAG, "FALHA ao criar instância do encoder!");
        // Prosseguir sem encoder, ou abortar dependendo da criticidade
    } else {
        ESP_LOGI(TAG, "Encoder criado com sucesso em GPIO %d e %d.", enc_config.pin_a, enc_config.pin_b);
        // Call to encoder_enable_acceleration is removed as it no longer exists.
        // The initial state of acceleration is set by enc_config.acceleration_enabled.
    }

    // ESP_LOGI(TAG, "Inicializando input event manager...");
    // input_event_manager_handle_t evt_mgr = create_input_event_manager(integrated_queue, button_app_queue, encoder_event_queue, NULL);
    // if (evt_mgr == NULL) {
    //     ESP_LOGE(TAG, "FALHA ao criar input event manager! Funcionalidade integrada pode ser afetada.");
    // } else {
    //     ESP_LOGI(TAG, "Input event manager criado com sucesso.");
    // }

    ESP_LOGI(TAG, "Criando central event processor task...");
    central_processor_params_t *cep_params = (central_processor_params_t *)malloc(sizeof(central_processor_params_t));
    if (cep_params == NULL) {
        ESP_LOGE(TAG, "FALHA ao alocar memoria para central_processor_params! Abortando criacao da task.");
        // Handle error, perhaps return or skip dependent tasks
    } else {
        cep_params->integrated_q_handle = integrated_queue;
        cep_params->button_q_handle = button_app_queue;
        cep_params->encoder_q_handle = encoder_event_queue;
        // cep_params->espnow_q_handle = NULL; // For future

        if (xTaskCreate(central_event_processor_task, "central_proc_task", 3072, cep_params, 6, &central_processor_task_handle) != pdPASS) { // Priority 6 (higher than default adapters)
            ESP_LOGE(TAG, "FALHA ao criar central event processor task!");
            free(cep_params); // Free memory if task creation failed
            central_processor_task_handle = NULL; // Ensure handle is NULL on failure
        } else {
            ESP_LOGI(TAG, "Central event processor task criada com sucesso.");
        }
    }

    if (central_processor_task_handle != NULL) {
        ESP_LOGI(TAG, "Criando button adapter task...");
        adapter_task_params_t *btn_adapter_params = (adapter_task_params_t *)malloc(sizeof(adapter_task_params_t));
        if (btn_adapter_params == NULL) {
            ESP_LOGE(TAG, "FALHA ao alocar memoria para btn_adapter_params!");
        } else {
            btn_adapter_params->component_queue = button_app_queue;
            btn_adapter_params->central_task_handle = central_processor_task_handle;
            if (xTaskCreate(button_adapter_task, "btn_adapter_task", 2048, btn_adapter_params, 5, NULL) != pdPASS) {
                ESP_LOGE(TAG, "FALHA ao criar button adapter task!");
                free(btn_adapter_params);
            } else {
                ESP_LOGI(TAG, "Button adapter task criada com sucesso.");
            }
        }
    } else {
        ESP_LOGE(TAG, "Central processor task handle is NULL, nao foi possivel criar button adapter task.");
    }

    if (central_processor_task_handle != NULL) {
        ESP_LOGI(TAG, "Criando encoder adapter task...");
        adapter_task_params_t *enc_adapter_params = (adapter_task_params_t *)malloc(sizeof(adapter_task_params_t));
        if (enc_adapter_params == NULL) {
            ESP_LOGE(TAG, "FALHA ao alocar memoria para enc_adapter_params!");
        } else {
            enc_adapter_params->component_queue = encoder_event_queue;
            enc_adapter_params->central_task_handle = central_processor_task_handle;
            if (xTaskCreate(encoder_adapter_task, "enc_adapter_task", 2048, enc_adapter_params, 5, NULL) != pdPASS) {
                ESP_LOGE(TAG, "FALHA ao criar encoder adapter task!");
                free(enc_adapter_params);
            } else {
                ESP_LOGI(TAG, "Encoder adapter task criada com sucesso.");
            }
        }
    } else {
        ESP_LOGE(TAG, "Central processor task handle is NULL, nao foi possivel criar encoder adapter task.");
    }

    // Cria task que vai processar eventos do sistema (recebidos do app_button_event_handler_task)
    ESP_LOGI(TAG, "Criando task de eventos do sistema...");
    if (xTaskCreate(system_event_task, "sys_evt_task", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "FALHA ao criar task de eventos do sistema! Abortando...");
        return;
    }
    ESP_LOGI(TAG, "Task de eventos do sistema criada com sucesso.");

    // ESP_LOGI(TAG, "Criando task handler de eventos de botão...");
    // if (xTaskCreate(app_button_event_handler_task, "btn_handler_task", 2048, NULL, 5, NULL) != pdPASS) {
    //     ESP_LOGE(TAG, "FALHA ao criar task handler de eventos de botão! Abortando...");
    //     return;
    // }
    // ESP_LOGI(TAG, "Task handler de eventos de botão criada com sucesso.");

    // if (my_encoder && encoder_event_queue) {
    //     ESP_LOGI(TAG, "Criando task handler de eventos de encoder...");
    //     if (xTaskCreate(app_encoder_event_handler_task, "enc_handler_task", 2048, NULL, 5, NULL) != pdPASS) {
    //         ESP_LOGE(TAG, "FALHA ao criar task handler de eventos de encoder!");
    //     } else {
    //         ESP_LOGI(TAG, "Task handler de eventos de encoder criada com sucesso.");
    //     }
    // } else {
    //     ESP_LOGW(TAG, "Encoder ou sua fila não foram inicializados. Task de handler de encoder não será criada.");
    // }

    ESP_LOGI(TAG, "Criando task consumer de eventos integrados...");
    if (xTaskCreate(integrated_event_consumer_task, "int_evt_consumer_task", 3072, (void*)integrated_queue, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "FALHA ao criar task consumer de eventos integrados!");
    } else {
        ESP_LOGI(TAG, "Task consumer de eventos integrados criada com sucesso.");
    }

    ESP_LOGI(TAG, "Sistema inicializado! Teste os botões e o encoder:");
    ESP_LOGI(TAG, "  - Pressione rapidamente: clique simples");  
    ESP_LOGI(TAG, "  - Pressione 2x rapidamente: clique duplo");
    ESP_LOGI(TAG, "  - Mantenha por 1s: clique longo");
    ESP_LOGI(TAG, "  - Mantenha por 3s+: clique muito longo");

    // app_main termina aqui, FreeRTOS segue executando as tasks
    ESP_LOGI(TAG, "app_main finalizado, sistema rodando...");

    // Opcional: você pode adicionar mais botões aqui
    ESP_LOGI(TAG, "Criando segundo botão no GPIO 22...");
    button_t *btn2 = button_create(GPIO_NUM_22, button_app_queue);
    if (btn2) {
        ESP_LOGI(TAG, "Segundo botão criado no GPIO 22");
        // Pode configurar tempos diferentes para este botão se desejar
        // button_set_click_times(btn2, 250, 1200, 3500);
    } else {
        ESP_LOGE(TAG, "FALHA ao criar segundo botão no GPIO 22!");
        // Não precisa abortar tudo, o primeiro botão e tasks ainda podem funcionar
    }
    
    ESP_LOGI(TAG, "app_main finalizado, sistema rodando...");
}
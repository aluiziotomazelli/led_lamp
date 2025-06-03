#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log_level.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "button.h"
#include "system_events.h"
#include "encoder.h"

static const char *TAG = "MAIN";
static const char *TAG_BUTTON_HANDLER = "BTN_HANDLER";
static const char *TAG_ENCODER_HANDLER = "ENC_HANDLER";

QueueHandle_t button_app_queue = NULL;
QueueHandle_t encoder_event_queue = NULL;

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
    // Configura nível de log para debug
    esp_log_level_set("*", ESP_LOG_DEBUG);
    
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

    // Inicializa o componente Encoder
    ESP_LOGI(TAG, "Inicializando componente encoder...");
    encoder_handle_t my_encoder = NULL;
    encoder_config_t enc_config = {
        .pin_a = GPIO_NUM_16,
        .pin_b = GPIO_NUM_17,
        .half_step_mode = true,
        .flip_direction = false,
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

        // Testar funções de configuração (opcional)
        esp_err_t err;
        err = encoder_enable_acceleration(my_encoder, true);
        ESP_LOGI(TAG, "Encoder enable acceleration: %s", esp_err_to_name(err));
        err = encoder_set_accel_params(my_encoder, 50, 5);
        ESP_LOGI(TAG, "Encoder set accel params: %s", esp_err_to_name(err));
        err = encoder_set_half_steps(my_encoder, true);
        ESP_LOGI(TAG, "Encoder set half steps: %s", esp_err_to_name(err));
    }

    // Cria task que vai processar eventos do sistema (recebidos do app_button_event_handler_task)
    ESP_LOGI(TAG, "Criando task de eventos do sistema...");
    if (xTaskCreate(system_event_task, "sys_evt_task", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "FALHA ao criar task de eventos do sistema! Abortando...");
        return;
    }
    ESP_LOGI(TAG, "Task de eventos do sistema criada com sucesso.");

    // Cria a task que lida com eventos brutos dos botões
    ESP_LOGI(TAG, "Criando task handler de eventos de botão...");
    if (xTaskCreate(app_button_event_handler_task, "btn_handler_task", 2048, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "FALHA ao criar task handler de eventos de botão! Abortando...");
        return;
    }
    ESP_LOGI(TAG, "Task handler de eventos de botão criada com sucesso.");

    // Cria a task que lida com eventos brutos do encoder
    if (my_encoder && encoder_event_queue) {
        ESP_LOGI(TAG, "Criando task handler de eventos de encoder...");
        if (xTaskCreate(app_encoder_event_handler_task, "enc_handler_task", 2048, NULL, 5, NULL) != pdPASS) {
            ESP_LOGE(TAG, "FALHA ao criar task handler de eventos de encoder!");
            // Não necessariamente abortar tudo, botões ainda podem funcionar
        } else {
            ESP_LOGI(TAG, "Task handler de eventos de encoder criada com sucesso.");
        }
    } else {
        ESP_LOGW(TAG, "Encoder ou sua fila não foram inicializados. Task de handler de encoder não será criada.");
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
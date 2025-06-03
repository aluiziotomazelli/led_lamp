#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log_level.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "button.h"
#include "system_events.h"

static const char *TAG = "MAIN";
static const char *TAG_BUTTON_HANDLER = "BTN_HANDLER";

QueueHandle_t button_app_queue = NULL;

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

    ESP_LOGI(TAG, "Sistema inicializado! Teste os botões:");
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
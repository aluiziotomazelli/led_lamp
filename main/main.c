#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log_level.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "button.h"
#include "system_events.h"

const char *TAG = "MAIN";

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

    // Cria botão no GPIO 23
    ESP_LOGI(TAG, "Criando botão no GPIO 23...");
    button_t *btn = button_create(GPIO_NUM_23);
    if (!btn) {
        ESP_LOGE(TAG, "FALHA ao criar botão! Abortando...");
        return;
    }

    ESP_LOGI(TAG, "Botão criado com sucesso!");

    // Configura tempos personalizados (opcional)
    ESP_LOGI(TAG, "Configurando tempos do botão...");
    button_set_click_times(btn, 200, 1000, 3000);
    
    ESP_LOGI(TAG, "Tempos configurados:");
    ESP_LOGI(TAG, "  - Timeout duplo clique: 200ms");
    ESP_LOGI(TAG, "  - Clique longo: 1000ms");
    ESP_LOGI(TAG, "  - Clique muito longo: 3000ms");

    // Cria task que vai processar eventos do sistema
    ESP_LOGI(TAG, "Criando task de eventos do sistema...");
    xTaskCreate(system_event_task, "sys_evt", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Sistema inicializado! Teste o botão:");
    ESP_LOGI(TAG, "  - Pressione rapidamente: clique simples");  
    ESP_LOGI(TAG, "  - Pressione 2x rapidamente: clique duplo");
    ESP_LOGI(TAG, "  - Mantenha por 1s: clique longo");
    ESP_LOGI(TAG, "  - Mantenha por 3s+: clique muito longo");

    // app_main termina aqui, FreeRTOS segue executando as tasks
    ESP_LOGI(TAG, "app_main finalizado, sistema rodando...");

    // Opcional: você pode adicionar mais botões aqui
    
    button_t *btn2 = button_create(GPIO_NUM_22);
    if (btn2) {
        ESP_LOGI(TAG, "Segundo botão criado no GPIO 22");
    }
    
}
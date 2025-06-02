#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "esp_log_level.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "button.h" 

const char *TAG = "MAIN";

void button_event_task(void *param) {
    button_t *btn = (button_t *)param;
    button_event_t evt;

    while (1) {
        if (xQueueReceive(button_get_event_queue(btn), &evt, portMAX_DELAY)) {
            switch (evt.type) {
                case BUTTON_CLICK:
                    ESP_LOGI(TAG, ">> Clique simples\n");
                    break;
                case BUTTON_DOUBLE_CLICK:
                    ESP_LOGI(TAG, ">> Clique duplo\n");
                    break;
                case BUTTON_LONG_CLICK:
                    ESP_LOGI(TAG, ">> Clique longo\n");
                    break;
                case BUTTON_VERY_LONG_CLICK:
                    ESP_LOGI(TAG, ">> Clique muito longo\n");
                    break;
                case BUTTON_TIMEOUT:
                    ESP_LOGW(TAG, ">> Timeout de clique\n");
                    break;
                case BUTTON_ERROR:
                    ESP_LOGE(TAG, ">> Erro no botão\n");
                    break;
                default:
                    ESP_LOGW(TAG, ">> Evento desconhecido: %d\n", evt.type);
                    break;
            }
        }
    }
}

void app_main(void) {
    esp_log_level_set("*", ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "Inicializando botão...");

    // Cria botão no GPIO 23
    button_t *btn = button_create(GPIO_NUM_23);
    if (!btn) {
        ESP_LOGW(TAG, "Falha ao criar botão");
        return;
    }

    // Configura tempos personalizados (opcional)
    button_set_click_times(btn, 200, 1000, 3000);

    // Cria task que vai escutar eventos da fila
    xTaskCreate(button_event_task, "btn_evt", 2048, btn, 5, NULL);

    // app_main termina aqui, FreeRTOS segue com tasks
}

#include "touch_button.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "driver/touch_pad.h"
#include "project_config.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
static const char *TAG = "TouchButton";

// Estado do touch button
typedef enum {
    TOUCH_WAIT_FOR_PRESS,
    TOUCH_DEBOUNCE_PRESS,
    TOUCH_WAIT_FOR_RELEASE,
    TOUCH_LONG_PRESS_DETECTED
} touch_button_state_t;

// Estrutura interna completa
struct touch_button_s {
    touch_pad_t touch_pad;
    touch_button_state_t state;
    uint32_t press_start_time_ms;
    uint32_t last_sample_time_ms;
    uint32_t last_repeat_time_ms;    // Para controlar repetições
    bool long_press_sent;
    
    // Configurações
    uint16_t threshold_value;
    uint16_t debounce_ms;
    uint16_t long_press_ms;
    uint16_t hold_repeat_ms;         // Intervalo de repetição
    uint16_t sample_interval_ms;
    
    QueueHandle_t output_queue;
    TaskHandle_t task_handle;
};

// Variável estática para controle de inicialização do touch pad
static bool touch_pad_initialized = false;

// Utilitário de tempo
static uint32_t get_current_time_ms() { 
    return esp_timer_get_time() / 1000; 
}

// Inicializa o sistema de touch pad (apenas uma vez)
static esp_err_t init_touch_pad_system() {
    if (touch_pad_initialized) {
        return ESP_OK;
    }
    
    esp_err_t ret = touch_pad_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch pad init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configurar parâmetros gerais do touch pad
    ret = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch pad set voltage failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    touch_pad_initialized = true;
    ESP_LOGI(TAG, "Touch pad system initialized");
    return ESP_OK;
}

// Calibra o touch pad e define o threshold
static esp_err_t calibrate_touch_pad(touch_button_t *btn, float threshold_percent) {
    uint16_t baseline_value;
    
    ESP_LOGI(TAG, "Calibrating touch pad %d...", btn->touch_pad);
    
    // Fazer algumas leituras para estabilizar
    for (int i = 0; i < 10; i++) {
        esp_err_t ret = touch_pad_read_filtered(btn->touch_pad, &baseline_value);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read during calibration %d: %s", i, esp_err_to_name(ret));
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Ler valor de referência final (sem toque)
    esp_err_t ret = touch_pad_read_filtered(btn->touch_pad, &baseline_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read touch pad baseline: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Calcular threshold baseado no percentual do valor de referência
    btn->threshold_value = (uint16_t)(baseline_value * threshold_percent);
    
    ESP_LOGI(TAG, "Touch pad %d calibrated - Baseline: %d, Threshold: %d (%.1f%%)", 
             btn->touch_pad, baseline_value, btn->threshold_value, threshold_percent * 100.0f);
    
    return ESP_OK;
}

// Função principal de detecção de toque
static touch_button_type_t touch_button_get_event(touch_button_t *btn) {
    uint32_t now = get_current_time_ms();
    uint16_t touch_value;
    
    // Ler valor do touch pad sempre (remover limitação de sample_interval aqui)
    esp_err_t ret = touch_pad_read_filtered(btn->touch_pad, &touch_value);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read touch pad %d: %s", btn->touch_pad, esp_err_to_name(ret));
        return TOUCH_ERROR;
    }
    
    bool is_touched = (touch_value < btn->threshold_value);
    
    ESP_LOGD(TAG, "Touch pad %d: value=%d, threshold=%d, touched=%s, state=%d", 
             btn->touch_pad, touch_value, btn->threshold_value, 
             is_touched ? "YES" : "NO", btn->state);
    
    switch (btn->state) {
        case TOUCH_WAIT_FOR_PRESS:
            if (is_touched) {
                btn->press_start_time_ms = now;
                btn->state = TOUCH_DEBOUNCE_PRESS;
                btn->long_press_sent = false;
                ESP_LOGD(TAG, "Touch pad %d: Starting debounce", btn->touch_pad);
            }
            break;
            
        case TOUCH_DEBOUNCE_PRESS:
            if (now - btn->press_start_time_ms > btn->debounce_ms) {
                if (is_touched) {
                    btn->state = TOUCH_WAIT_FOR_RELEASE;
                    ESP_LOGD(TAG, "Touch pad %d: Press confirmed", btn->touch_pad);
                } else {
                    // Falso positivo durante debounce
                    btn->state = TOUCH_WAIT_FOR_PRESS;
                    ESP_LOGD(TAG, "Touch pad %d: False positive during debounce", btn->touch_pad);
                }
            }
            break;
            
        case TOUCH_WAIT_FOR_RELEASE:
            if (!is_touched) {
                // Toque liberado
                uint32_t press_duration = now - btn->press_start_time_ms;
                btn->state = TOUCH_WAIT_FOR_PRESS;
                
                if (btn->long_press_sent) {
                    // Já enviou long press, não enviar mais nada
                    ESP_LOGD(TAG, "Touch pad %d: Release after long press", btn->touch_pad);
                    return TOUCH_NONE;
                } else if (press_duration >= btn->long_press_ms) {
                    // Era um long press mas ainda não foi enviado
                    ESP_LOGD(TAG, "Touch pad %d: Long press on release (%lu ms)", 
                             btn->touch_pad, press_duration);
                    return TOUCH_LONG_PRESS;
                } else {
                    // Toque normal
                    ESP_LOGD(TAG, "Touch pad %d: Normal press (%lu ms)", 
                             btn->touch_pad, press_duration);
                    return TOUCH_PRESS;
                }
            } else {
                // Ainda tocando, verificar se é long press
                uint32_t press_duration = now - btn->press_start_time_ms;
                if (press_duration >= btn->long_press_ms && !btn->long_press_sent) {
                    btn->long_press_sent = true;
                    btn->last_repeat_time_ms = now; // Inicializar tempo de repetição
                    btn->state = TOUCH_LONG_PRESS_DETECTED;
                    ESP_LOGD(TAG, "Touch pad %d: Long press detected (%lu ms)", 
                             btn->touch_pad, press_duration);
                    return TOUCH_LONG_PRESS;
                }
            }
            break;
            
        case TOUCH_LONG_PRESS_DETECTED:
            if (!is_touched) {
                // Liberou após long press
                btn->state = TOUCH_WAIT_FOR_PRESS;
                ESP_LOGD(TAG, "Touch pad %d: Released after long press", btn->touch_pad);
            } else {
                // Ainda segurando - enviar TOUCH_LONG_PRESS repetidamente
                if (now - btn->last_repeat_time_ms >= btn->hold_repeat_ms) {
                    btn->last_repeat_time_ms = now;
                    ESP_LOGD(TAG, "Touch pad %d: Long press repeat", btn->touch_pad);
                    return TOUCH_LONG_PRESS;
                }
            }
            break;
    }
    
    return TOUCH_NONE;
}

// Task individual de cada touch button
static void touch_button_task(void *param) {
    touch_button_t *btn = (touch_button_t *)param;
    
    ESP_LOGI(TAG, "Touch button task started for pad %d", btn->touch_pad);
    
    // Aguardar um pouco para estabilização completa
    vTaskDelay(pdMS_TO_TICKS(500));
    
    while (1) {
        touch_button_type_t event_type = touch_button_get_event(btn);
        
        if (event_type != TOUCH_NONE) {
            touch_button_event_t event;
            event.type = event_type;
            event.touch_pad = btn->touch_pad;
            
            if (xQueueSend(btn->output_queue, &event, pdMS_TO_TICKS(10)) == pdPASS) {
                ESP_LOGD(TAG, "Touch pad %d: event %d sent to queue", btn->touch_pad, event_type);
            } else {
                ESP_LOGW(TAG, "Touch pad %d: failed to send event %d to queue", 
                         btn->touch_pad, event_type);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Reduzir delay para ser mais responsivo
    }
}

// Criação do touch button
touch_button_t *touch_button_create(const touch_button_config_t *config, 
                                   QueueHandle_t output_queue) {
    if (!config || !output_queue) {
        ESP_LOGE(TAG, "Invalid arguments: config or output_queue is NULL");
        return NULL;
    }
    
    // Verificar se o touch pad é válido
    if (config->touch_pad >= TOUCH_PAD_MAX) {
        ESP_LOGE(TAG, "Invalid touch pad number: %d", config->touch_pad);
        return NULL;
    }
    
    // Verificar se o threshold está em range válido
    if (config->threshold_percent <= 0.0f || config->threshold_percent >= 1.0f) {
        ESP_LOGE(TAG, "Invalid threshold percent: %f (must be between 0.0 and 1.0)", 
                 config->threshold_percent);
        return NULL;
    }
    
    // Inicializar sistema de touch pad se necessário
    if (init_touch_pad_system() != ESP_OK) {
        return NULL;
    }
    
    touch_button_t *btn = calloc(1, sizeof(touch_button_t));
    if (!btn) {
        ESP_LOGE(TAG, "Failed to allocate memory for touch button struct");
        return NULL;
    }
    
    btn->touch_pad = config->touch_pad;
    btn->output_queue = output_queue;
    btn->state = TOUCH_WAIT_FOR_PRESS;
    btn->press_start_time_ms = 0;
    btn->last_sample_time_ms = 0;
    btn->last_repeat_time_ms = 0;
    btn->long_press_sent = false;
    
    // Configurar parâmetros com valores padrão se necessário
    btn->debounce_ms = (config->debounce_ms > 0) ? config->debounce_ms : 50;
    btn->long_press_ms = (config->long_press_ms > 0) ? config->long_press_ms : 1000;
    btn->hold_repeat_ms = (config->hold_repeat_ms > 0) ? config->hold_repeat_ms : 200;
    btn->sample_interval_ms = (config->sample_interval_ms > 0) ? config->sample_interval_ms : 100;
    
    // Configurar o touch pad
    esp_err_t ret = touch_pad_config(btn->touch_pad, 0); // 0 = usar configuração padrão
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch pad config failed for pad %d: %s", 
                 btn->touch_pad, esp_err_to_name(ret));
        free(btn);
        return NULL;
    }
    
    // Iniciar o filtro
    ret = touch_pad_filter_start(10); // 10ms filter period
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Touch pad filter start failed: %s", esp_err_to_name(ret));
        free(btn);
        return NULL;
    }
    
    // Aguardar estabilização do filtro
    vTaskDelay(pdMS_TO_TICKS(300));
    
    ESP_LOGI(TAG, "Starting calibration for touch pad %d", btn->touch_pad);
    
    // Calibrar o touch pad
    if (calibrate_touch_pad(btn, config->threshold_percent) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to calibrate touch pad %d", btn->touch_pad);
        free(btn);
        return NULL;
    }
    
    // Criar a task
    BaseType_t res = xTaskCreate(touch_button_task, "touch_button_task", 
                                TOUCH_BUTTON_TASK_STACK_SIZE, btn, 5, &btn->task_handle);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create touch button task for pad %d", btn->touch_pad);
        free(btn);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Touch button created on pad %d (threshold: %d, debounce: %dms, long_press: %dms)", 
             btn->touch_pad, btn->threshold_value, btn->debounce_ms, btn->long_press_ms);
    
    return btn;
}

void touch_button_delete(touch_button_t *btn) {
    if (btn) {
        if (btn->task_handle) {
            vTaskDelete(btn->task_handle);
            ESP_LOGI(TAG, "Touch button task for pad %d deleted", btn->touch_pad);
        }
        
        ESP_LOGI(TAG, "Touch button on pad %d deleted", btn->touch_pad);
        free(btn);
    }
}
#include "touch_button.h"
#include "driver/touch_pad.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "project_config.h"
#include <stdint.h>

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
    uint32_t last_repeat_time_ms;
    bool long_press_sent;
    bool detection_active;  // Novo campo para controle de ativação

    // Configurações
    uint16_t threshold_value;
    float threshold_percent;
    uint32_t last_recalibration_time_ms;
    uint32_t pending_recalibration_since;  // Novo campo para recalibração pendente
    uint16_t debounce_ms;
    uint16_t long_press_ms;
    uint16_t hold_repeat_ms;
    uint16_t sample_interval_ms;
    uint32_t recalibration_interval_ms;

    QueueHandle_t output_queue;
    TaskHandle_t task_handle;
};

static bool touch_pad_initialized = false;

static bool is_button_idle(touch_button_t *btn) {
    // Verificação rápida do estado primeiro
    if (btn->state != TOUCH_WAIT_FOR_PRESS) {
        return false;
    }
    
    // Confirmação com leitura física
    uint16_t touch_value;
    if (touch_pad_read_filtered(btn->touch_pad, &touch_value) != ESP_OK) {
        return false;
    }
    
    return (touch_value >= btn->threshold_value);
}

static uint32_t get_current_time_ms() {
    return esp_timer_get_time() / 1000;
}

static esp_err_t init_touch_pad_system() {
    if (touch_pad_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = touch_pad_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch pad init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch pad set voltage failed: %s", esp_err_to_name(ret));
        return ret;
    }

    touch_pad_initialized = true;
    ESP_LOGI(TAG, "Touch pad system initialized");
    return ESP_OK;
}

static esp_err_t pause_detection(touch_button_t *btn) {
    if (!btn) return ESP_ERR_INVALID_ARG;
    btn->detection_active = false;
    return touch_pad_config(btn->touch_pad, TOUCH_PAD_HIGH_VOLTAGE_THRESHOLD);
}

static esp_err_t resume_detection(touch_button_t *btn) {
    if (!btn) return ESP_ERR_INVALID_ARG;
    esp_err_t ret = touch_pad_config(btn->touch_pad, 0);
    if (ret == ESP_OK) {
        btn->detection_active = true;
    }
    return ret;
}

static esp_err_t calibrate_touch_pad(touch_button_t *btn, float threshold_percent) {
    uint16_t baseline_value;

    ESP_LOGI(TAG, "Calibrating touch pad %d...", btn->touch_pad);

    // Pausar detecção durante calibração
    pause_detection(btn);

    // Fazer algumas leituras para estabilizar
    for (int i = 0; i < 10; i++) {
        esp_err_t ret = touch_pad_read_filtered(btn->touch_pad, &baseline_value);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to read during calibration %d: %s", i, esp_err_to_name(ret));
            resume_detection(btn);
            return ret;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    esp_err_t ret = touch_pad_read_filtered(btn->touch_pad, &baseline_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read touch pad baseline: %s", esp_err_to_name(ret));
        resume_detection(btn);
        return ret;
    }

    btn->threshold_value = (uint16_t)(baseline_value * threshold_percent);

    ESP_LOGI(TAG, "Touch pad %d calibrated - Baseline: %d, Threshold: %d (%.1f%%)",
             btn->touch_pad, baseline_value, btn->threshold_value, threshold_percent * 100.0f);

    // Reativar detecção após calibração
    return resume_detection(btn);
}

static touch_button_type_t touch_button_get_event(touch_button_t *btn) {
    if (!btn->detection_active) {
        return TOUCH_NONE;
    }

    uint32_t now = get_current_time_ms();
    uint16_t touch_value;

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
            }
            break;
            
        case TOUCH_DEBOUNCE_PRESS:
            if (now - btn->press_start_time_ms > btn->debounce_ms) {
                if (is_touched) {
                    btn->state = TOUCH_WAIT_FOR_RELEASE;
                } else {
                    btn->state = TOUCH_WAIT_FOR_PRESS;
                }
            }
            break;
            
        case TOUCH_WAIT_FOR_RELEASE:
            if (!is_touched) {
                uint32_t press_duration = now - btn->press_start_time_ms;
                btn->state = TOUCH_WAIT_FOR_PRESS;

                if (btn->long_press_sent) {
                    return TOUCH_NONE;
                } else if (press_duration >= btn->long_press_ms) {
                    return TOUCH_HOLD_PRESS;
                } else {
                    return TOUCH_PRESS;
                }
            } else {
                uint32_t press_duration = now - btn->press_start_time_ms;
                if (press_duration >= btn->long_press_ms && !btn->long_press_sent) {
                    btn->long_press_sent = true;
                    btn->last_repeat_time_ms = now;
                    btn->state = TOUCH_LONG_PRESS_DETECTED;
                    return TOUCH_HOLD_PRESS;
                }
            }
            break;
            
        case TOUCH_LONG_PRESS_DETECTED:
            if (!is_touched) {
                btn->state = TOUCH_WAIT_FOR_PRESS;
            } else if (now - btn->last_repeat_time_ms >= btn->hold_repeat_ms) {
                btn->last_repeat_time_ms = now;
                return TOUCH_HOLD_PRESS;
            }
            break;
    }
    
    return TOUCH_NONE;
}

static void touch_button_task(void *param) {
    touch_button_t *btn = (touch_button_t *)param;

    ESP_LOGI(TAG, "Touch button task started for pad %d", btn->touch_pad);
    vTaskDelay(pdMS_TO_TICKS(500));

    btn->last_recalibration_time_ms = get_current_time_ms();
    btn->pending_recalibration_since = 0;
    btn->detection_active = true;

    while (1) {
        uint32_t now = get_current_time_ms();

        // Verificação de recalibração
        if (now - btn->last_recalibration_time_ms >= btn->recalibration_interval_ms || 
            (btn->pending_recalibration_since && 
             now - btn->pending_recalibration_since > 60000)) { // 60 segundos máximo de espera
            
            uint16_t touch_value;
            esp_err_t ret = touch_pad_read_filtered(btn->touch_pad, &touch_value);
            
            if (is_button_idle(btn)) {
                ESP_LOGI(TAG, "Recalibrating touch pad %d...", btn->touch_pad);
                if (calibrate_touch_pad(btn, btn->threshold_percent) == ESP_OK) {
                    btn->last_recalibration_time_ms = now;
                    btn->pending_recalibration_since = 0;
                    ESP_LOGI(TAG, "Touch pad %d recalibrated successfully", btn->touch_pad);
                } else {
                    ESP_LOGE(TAG, "Failed to recalibrate touch pad %d", btn->touch_pad);
                }
            } else {
                if (!btn->pending_recalibration_since) {
                    btn->pending_recalibration_since = now;
                }
                ESP_LOGD(TAG, "Delaying recalibration on pad %d (button in use)", btn->touch_pad);
            }
        }

        // Processamento normal de eventos
        touch_button_type_t event_type = touch_button_get_event(btn);
        if (event_type != TOUCH_NONE) {
            touch_button_event_t event = {
                .type = event_type,
                .touch_pad = btn->touch_pad
            };
            xQueueSend(btn->output_queue, &event, pdMS_TO_TICKS(10));
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

touch_button_t *touch_button_create(const touch_button_config_t *config, QueueHandle_t output_queue) {
    if (!config || !output_queue) {
        ESP_LOGE(TAG, "Invalid arguments");
        return NULL;
    }

    if (config->touch_pad >= TOUCH_PAD_MAX) {
        ESP_LOGE(TAG, "Invalid touch pad number: %d", config->touch_pad);
        return NULL;
    }

    if (config->threshold_percent <= 0.0f || config->threshold_percent >= 1.0f) {
        ESP_LOGE(TAG, "Invalid threshold percent: %f", config->threshold_percent);
        return NULL;
    }

    if (init_touch_pad_system() != ESP_OK) {
        return NULL;
    }

    touch_button_t *btn = calloc(1, sizeof(touch_button_t));
    if (!btn) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }

    // Inicialização dos campos
    btn->touch_pad = config->touch_pad;
    btn->output_queue = output_queue;
    btn->state = TOUCH_WAIT_FOR_PRESS;
    btn->threshold_percent = config->threshold_percent;
    btn->recalibration_interval_ms = TOUCH_RECALIBRATION_INTERVAL_MIN * 60 * 1000;
    btn->debounce_ms = (config->debounce_ms > 0) ? config->debounce_ms : 50;
    btn->long_press_ms = (config->long_press_ms > 0) ? config->long_press_ms : 1000;
    btn->hold_repeat_ms = (config->hold_repeat_ms > 0) ? config->hold_repeat_ms : 200;
    btn->sample_interval_ms = (config->sample_interval_ms > 0) ? config->sample_interval_ms : 100;

    // Configuração do hardware
    esp_err_t ret = touch_pad_config(btn->touch_pad, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch pad config failed: %s", esp_err_to_name(ret));
        free(btn);
        return NULL;
    }

    ret = touch_pad_filter_start(10);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Touch pad filter start failed: %s", esp_err_to_name(ret));
        free(btn);
        return NULL;
    }

    vTaskDelay(pdMS_TO_TICKS(300));

    // Calibração inicial
    if (calibrate_touch_pad(btn, config->threshold_percent) != ESP_OK) {
        free(btn);
        return NULL;
    }

    // Criação da task
    BaseType_t task_res = xTaskCreate(touch_button_task, "touch_button_task",
                                    TOUCH_BUTTON_TASK_STACK_SIZE, btn, 5, &btn->task_handle);
    if (task_res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        free(btn);
        return NULL;
    }

    ESP_LOGI(TAG, "Touch button created on pad %d", btn->touch_pad);
    return btn;
}

void touch_button_delete(touch_button_t *btn) {
    if (btn) {
        if (btn->task_handle) {
            vTaskDelete(btn->task_handle);
        }
        free(btn);
    }
}
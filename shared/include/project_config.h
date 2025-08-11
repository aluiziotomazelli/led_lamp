#pragma once

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include "esp_log.h"
#include "esp_debug_helpers.h"


#define BUTTON1_PIN 23
#define ENCODER_PIN_A 17
#define ENCODER_PIN_B 16

#define BUTTON_QUEUE_SIZE 5
#define ENCODER_QUEUE_SIZE 10
#define ESPNOW_QUEUE_SIZE 10

#define BUTTON_TASK_STACK_SIZE 2044
#define ENCODER_TASK_STACK_SIZE 3072
#define TOUCH_BUTTON_TASK_STACK_SIZE 2048

// Touch Button Configuration
#define TOUCH_BUTTON_PIN TOUCH_PAD_NUM0    // GPIO4
#define TOUCH_THRESHOULD 0.6f
#define TOUCH_BUTTON_QUEUE_SIZE 10
#define TOUCH_BUTTON_TASK_STACK_SIZE 2048

// Touch Button Timing (opcional - valores padrão serão usados se não definidos)
#define TOUCH_DEBOUNCE_MS       50    // Debounce time
#define TOUCH_HOLD_PRESS_MS     1000  // Long press time
#define TOUCH_HOLD_REPEAT_MS    300  // Long press time

#define TOUCH_SAMPLE_INTERVAL   100   // Sample interval
#define TOUCH_RECALIBRATION_INTERVAL_MIN 10 //tempo em minutos

// Bot]ao físico
#define DEBOUNCE_PRESS_MS 50
#define DEBOUNCE_RELEASE_MS 30
#define DOUBLE_CLICK_MS 180
#define LONG_CLICK_MS 1000
#define VERY_LONG_CLICK_MS 3000

#define ENC_ACCEL_GAP 50
#define MAX_ACCEL_MULTIPLIER 5

// Configurações da FSM
#define FSM_STACK_SIZE      4096
#define FSM_PRIORITY        5
#define FSM_TIMEOUT_MS      100
#define FSM_MODE_TIMEOUT_MS 30000




#ifdef __cplusplus
extern "C" {
#endif

// Implementação segura para todo o projeto
#ifndef configASSERT
#define configASSERT(x) do { \
    if (!(x)) { \
        ESP_LOGE("ASSERT", "Failed at %s:%d (%s)", __FILE__, __LINE__, #x); \
        if (CONFIG_ESP_SYSTEM_PANIC_PRINT_BACKTRACE) { \
            esp_backtrace_dump(); \
        } \
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); } \
    } \
} while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif // PROJECT_CONFIG_H
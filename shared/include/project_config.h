#pragma once

#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#include "esp_log.h"
#include "esp_debug_helpers.h"


#define BUTTON1_PIN 23
#define ENCODER_PIN_A 17
#define ENCODER_PIN_B 18

#define BUTTON_QUEUE_SIZE 5
#define ENCODER_QUEUE_SIZE 10
#define ESPNOW_QUEUE_SIZE 10

#define BUTTON_TASK_STACK_SIZE 2044
#define ENCODER_TASK_STACK_SIZE 3072



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
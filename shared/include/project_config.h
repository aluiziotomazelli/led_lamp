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

// LED Strip Configuration
#define LED_STRIP_NUM_LEDS          8  // Example: 8 LEDs
#define LED_STRIP_SPI_HOST          SPI2_HOST // Example: VSPI (SPI3_HOST is also common)
#define LED_STRIP_SPI_CLK_SPEED_HZ  (10 * 1000 * 1000) // 10 MHz
#define LED_STRIP_GPIO_MOSI         19 // Define your MOSI GPIO pin
#define LED_STRIP_GPIO_SCLK         18 // Define your SCLK GPIO pin


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
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Tipos para eventos de clique de botão
typedef enum {
    BUTTON_NONE_CLICK,          ///< Nenhum clique detectado
    BUTTON_CLICK,               ///< Clique simples
    BUTTON_DOUBLE_CLICK,        ///< Clique duplo
    BUTTON_LONG_CLICK,          ///< Clique longo
    BUTTON_VERY_LONG_CLICK,     ///< Clique muito longo
    BUTTON_TIMEOUT,             ///< Timeout de clique
    BUTTON_ERROR                ///< Estado de erro
} button_click_type_t;

typedef struct {
    button_click_type_t type;   ///< Type of the click detected
    gpio_num_t pin;             ///< GPIO pin of the button
} button_event_t;

// Declaração incompleta para esconder implementação interna
typedef struct button_s button_t;

// Configuration structure for button creation
typedef struct {
    gpio_num_t pin;
    bool active_low;                // True if a press is LOW, false if HIGH
    uint16_t debounce_press_ms;
    uint16_t debounce_release_ms;
    uint16_t double_click_ms;       // Max time between clicks for a double click
    uint16_t long_click_ms;         // Min time for a long click
    uint16_t very_long_click_ms;    // Min time for a very long click
} button_config_t;

// Cria uma nova instância de botão, configurada no pino dado.
// Retorna ponteiro para button_t ou NULL em erro.
button_t *button_create(const button_config_t* config, QueueHandle_t output_queue);

// Libera recursos associados ao botão (se implementar)
void button_delete(button_t *btn);
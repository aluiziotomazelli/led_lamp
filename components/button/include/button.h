#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Definições padrão (pode ajustar)
#define DEBOUNCE_PRESS_MS 40
#define DEBOUNCE_RELEASE_MS 60
#define DOUBLE_CLICK_MS 200
#define LONG_CLICK_MS 1000
#define VERY_LONG_CLICK_MS 3000

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
    button_click_type_t type;   ///< Tipo do clique detectado
} button_event_t;

// Declaração incompleta para esconder implementação interna
typedef struct button_s button_t;

// Cria uma nova instância de botão, configurada no pino dado.
// Retorna ponteiro para button_t ou NULL em erro.
button_t *button_create(gpio_num_t pin);

// Obtém a fila de eventos do botão para ler cliques.
// Pode ser usado com xQueueReceive para aguardar eventos.
QueueHandle_t button_get_event_queue(button_t *btn);

// Ajusta tempos de debounce (opcional)
void button_set_debounce(button_t *btn, uint16_t debounce_press_ms, uint16_t debounce_release_ms);

// Ajusta tempos de clique (opcional)
void button_set_click_times(button_t *btn, uint16_t double_click_ms, uint16_t long_click_ms, uint16_t very_long_click_ms);

// Libera recursos associados ao botão (se implementar)
void button_delete(button_t *btn);

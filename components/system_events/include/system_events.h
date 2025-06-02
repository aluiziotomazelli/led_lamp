#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// Tipos de eventos - compatível com button_click_type_t
typedef enum {
    EVENT_NONE = 0, // Evento vazio, usado para inicialização
    EVENT_BUTTON_CLICK,
    EVENT_BUTTON_DOUBLE_CLICK,
    EVENT_BUTTON_LONG_CLICK,
    EVENT_BUTTON_VERY_LONG_CLICK,
    EVENT_BUTTON_TIMEOUT,
    EVENT_BUTTON_ERROR,
    EVENT_ENCODER_ROTATION
} event_type_t;

// Dados do botão
typedef struct {
    uint8_t button_id;
} button_data_t;

// Dados do encoder  
typedef struct {
    int32_t steps;
} encoder_data_t;

// Estrutura de evento unificada (sem timer que não vai usar)
typedef struct {
    event_type_t type;
    union {
        encoder_data_t encoder;
        button_data_t button;
    } data;
} system_event_t;

// API super simples - só o essencial
void system_events_init(void);
bool system_event_send(const system_event_t *event);
bool system_event_receive(system_event_t *event, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
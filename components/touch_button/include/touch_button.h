#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/touch_pad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Tipos para eventos de toque
typedef enum {
    TOUCH_NONE,          ///< Nenhum toque detectado
    TOUCH_PRESS,         ///< Toque simples (press/release)
    TOUCH_LONG_PRESS,    ///< Toque longo (press and hold) - repetido durante hold
    TOUCH_ERROR          ///< Estado de erro
} touch_button_type_t;

typedef struct {
    touch_button_type_t type;   ///< Tipo do toque detectado
    touch_pad_t touch_pad;      ///< Touch pad que gerou o evento
} touch_button_event_t;

// Declaração incompleta para esconder implementação interna
typedef struct touch_button_s touch_button_t;

// Estrutura de configuração para criação do touch button
typedef struct {
    touch_pad_t touch_pad;          ///< Touch pad a ser usado (TOUCH_PAD_NUM0 a TOUCH_PAD_NUM9)
    float threshold_percent;        ///< Percentual do valor de referência para trigger (0.0 a 1.0)
    uint16_t debounce_ms;          ///< Tempo de debounce em ms (padrão: 50ms)
    uint16_t long_press_ms;        ///< Tempo mínimo para long press em ms (padrão: 1000ms)
    uint16_t hold_repeat_ms;       ///< Intervalo de repetição durante hold em ms (padrão: 200ms)
    uint16_t sample_interval_ms;   ///< Intervalo de amostragem em ms (padrão: 100ms)
} touch_button_config_t;

/**
 * @brief Cria uma nova instância de touch button
 * @param config Configuração do touch button
 * @param output_queue Queue onde os eventos serão enviados
 * @return Ponteiro para touch_button_t ou NULL em caso de erro
 */
touch_button_t *touch_button_create(const touch_button_config_t* config, QueueHandle_t output_queue);

/**
 * @brief Libera recursos associados ao touch button
 * @param btn Ponteiro para o touch button a ser deletado
 */
void touch_button_delete(touch_button_t *btn);
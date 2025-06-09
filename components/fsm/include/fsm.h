#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "input_integrator.h"

typedef struct {
	QueueHandle_t input_queue;
    QueueHandle_t output_queue;
} fsm_queues_t;

// Estados da FSM
typedef enum {
    MODE_OFF,
    MODE_DISPLAY,
    MODE_EFFECT_CHANGE,
    MODE_EFFECT_SETUP,
    MODE_SYSTEM_SETUP
} fsm_state_t;

// Tipos de ações que a FSM pode disparar
typedef enum {
    ACTION_TURN_ON_LAMP,
    ACTION_TURN_OFF_LAMP,
    ACTION_ADJUST_BRIGHTNESS,
    ACTION_SELECT_EFFECT,
    ACTION_SHOW_EFFECT_PREVIEW,
    ACTION_ADJUST_EFFECT_PARAM,
    ACTION_SAVE_EFFECT_PARAM,
    ACTION_REVERT_EFFECT_PARAMS,
    ACTION_SAVE_CONFIG,
    ACTION_LOAD_CONFIG,
    ACTION_EXIT_SETUP,
    ACTION_FEEDBACK_SHORT,
    ACTION_FEEDBACK_LONG,
    ACTION_SELECT_PARAMETER
} fsm_action_type_t;

// Estrutura para ajuste de parâmetro de efeito
typedef struct {
    uint8_t param_id;
    int16_t delta;
} effect_param_adjust_t;

// Estrutura de ação de saída da FSM
typedef struct {
    fsm_action_type_t type;
    union {
        uint8_t brightness;
        uint8_t effect_id;
        uint8_t param_index;
        effect_param_adjust_t effect_param_adj;
        // Adicione outros payloads conforme necessário
    } data;
} fsm_output_action_t;

// Inicializa a FSM
void fsm_init(fsm_queues_t queues);

#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "input_integrator.h"
#include <stdint.h>




/**
 * @brief Estados da FSM
 */
typedef enum {
	FSM_STATE_OFF,
    FSM_STATE_DISPLAY,          ///< Modo exibição - mostra efeito ativo
    FSM_STATE_EFFECT_SELECT,    ///< Modo seleção de efeito
    FSM_STATE_EFFECT_SETUP,     ///< Modo configuração do efeito atual
    FSM_STATE_SYSTEM_SETUP      ///< Modo configuração do sistema
} fsm_state_t;

/**
 * @brief Configurações da FSM
 */
typedef struct {
    uint32_t task_stack_size;       ///< Tamanho da stack da task (default: 4096)
    UBaseType_t task_priority;      ///< Prioridade da task (default: 5)
    TickType_t queue_timeout_ms;    ///< Timeout para receber da queue (default: 100ms)
    uint32_t mode_timeout_ms;       ///< Timeout para voltar ao modo display (default: 30000ms)
} fsm_config_t;

/**
 * @brief Inicializa a FSM
 * 
 * @param queue_handle Handle da queue de eventos integrados
 * @param config Configurações da FSM (pode ser NULL para usar defaults)
 * @return esp_err_t ESP_OK se inicializado com sucesso
 */
esp_err_t fsm_init(QueueHandle_t queue_handle, const fsm_config_t *config);

/**
 * @brief Para a FSM (para cleanup)
 * 
 * @return esp_err_t ESP_OK se parado com sucesso
 */
esp_err_t fsm_deinit(void);

/**
 * @brief Verifica se a FSM está inicializada e rodando
 * 
 * @return true se inicializada e rodando
 */
bool fsm_is_running(void);

/**
 * @brief Obtém o estado atual da FSM
 * 
 * @return fsm_state_t Estado atual
 */
fsm_state_t fsm_get_current_state(void);

/**
 * @brief Obtém o efeito atualmente selecionado
 * 
 * @return uint8_t Índice do efeito atual
 */
uint8_t fsm_get_current_effect(void);

/**
 * @brief Obtém o brilho global atual
 * 
 * @return uint8_t Brilho atual (0-254)
 */
uint8_t fsm_get_global_brightness(void);

/**
 * @brief Verifica se a fita LED está ligada
 * 
 * @return true se ligada, false se desligada
 */
bool fsm_is_led_strip_on(void);

/**
 * @brief Estatísticas da FSM (para debug)
 */
typedef struct {
    uint32_t events_processed;      ///< Total de eventos processados
    uint32_t button_events;         ///< Eventos de botão processados
    uint32_t encoder_events;        ///< Eventos de encoder processados
    uint32_t espnow_events;         ///< Eventos ESP-NOW processados
    uint32_t state_transitions;     ///< Número de transições de estado
    uint32_t queue_timeouts;        ///< Timeouts na queue
    uint32_t invalid_events;        ///< Eventos inválidos para estado atual
    fsm_state_t current_state;      ///< Estado atual
    uint32_t time_in_current_state; ///< Tempo no estado atual (ms)
} fsm_stats_t;

/**
 * @brief Obtém estatísticas da FSM
 * 
 * @param stats Ponteiro para estrutura de estatísticas
 * @return esp_err_t ESP_OK se sucesso
 */
esp_err_t fsm_get_stats(fsm_stats_t *stats);

/**
 * @brief Reseta estatísticas da FSM
 */
void fsm_reset_stats(void);

/**
 * @brief Força transição para um estado específico (para debug/teste)
 * 
 * @param new_state Estado desejado
 * @return esp_err_t ESP_OK se transição válida
 */
esp_err_t fsm_force_state(fsm_state_t new_state);


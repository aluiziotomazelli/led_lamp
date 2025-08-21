/**
 * @file input_integrator.h
 * @brief Input event integrator for multiple input sources
 * @author Your Name
 * @version 1.0
 */

#pragma once

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Input device drivers
#include "button.h"
#include "encoder.h"
#include "touch.h"
#include "switch.h"
#include "espnow_controller.h"

/**
 * @brief ESPNOW event data structure, passed in the integrated queue
 */
typedef struct {
    uint8_t mac_addr[6];      ///< MAC address of sender
    espnow_message_t msg;     ///< The received message data
} espnow_event_t;

/**
 * @brief Enumeration of input event sources
 */
typedef enum {
    EVENT_SOURCE_BUTTON,    ///< Physical button event
    EVENT_SOURCE_ENCODER,   ///< Rotary encoder event
    EVENT_SOURCE_ESPNOW,    ///< ESPNOW wireless event
    EVENT_SOURCE_TOUCH,     ///< Touch sensor event
    EVENT_SOURCE_SWITCH     ///< Switch event
} event_source_t;

/**
 * @brief Integrated input event structure
 */
typedef struct {
    event_source_t source;   		///< Source of the event
    uint32_t timestamp;      		///< Timestamp when event was received
    union {
        button_event_t button;   	///< Button event data
        encoder_event_t encoder; 	///< Encoder event data
        espnow_event_t espnow;   	///< ESPNOW event data
        touch_event_t touch;     	///< Touch event data
        switch_event_t switch_evt; 	///< Switch event data
    } data;                     	///< Event payload
} integrated_event_t;

/**
 * @brief Queue management structure
 */
typedef struct {
    QueueHandle_t button_queue;      ///< Button events queue
    QueueHandle_t encoder_queue;     ///< Encoder events queue
    QueueHandle_t espnow_queue;      ///< ESPNOW events queue
    QueueHandle_t touch_queue;       ///< Touch events queue
    QueueHandle_t switch_queue;      ///< Switch events queue
    QueueHandle_t integrated_queue;  ///< Integrated output queue
    QueueSetHandle_t queue_set;      ///< Queue set for event monitoring
} queue_manager_t;

/**
 * @brief Initialize the queue manager
 * 
 * @param[in] btn_q Button events queue
 * @param[in] enc_q Encoder events queue
 * @param[in] espnow_q ESPNOW events queue
 * @param[in] touch_q Touch events queue
 * @param[in] switch_q Switch events queue
 * @param[in] int_q Integrated output queue
 * @return Initialized queue manager structure
 * 
 * @note All input queues must be created before calling this function
 * @warning Queue sizes must be properly configured in project_config.h
 */
#include "esp_err.h"

esp_err_t init_queue_manager(QueueHandle_t btn_q, QueueHandle_t enc_q,
                                 QueueHandle_t espnow_q, QueueHandle_t touch_q,
                                 QueueHandle_t switch_q, QueueHandle_t int_q);

/**
 * @brief Main input integration task
 * 
 * @param[in] pvParameters Pointer to queue manager structure
 * 
 * @note This task monitors multiple queues and integrates events into a single stream
 * @warning Task priority should be higher than individual input device tasks
 */
void integrator_task(void *pvParameters);
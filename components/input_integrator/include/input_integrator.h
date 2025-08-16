#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "button.h"
#include "encoder.h"
#include "touch.h"
#include "switch.h"

/**
 * @brief ESPNOW event data structure
 */
typedef struct {
    unsigned char mac_addr[6];  ///< MAC address of sender
    unsigned char *data;        ///< Received data pointer
    int data_len;               ///< Length of received data
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
    event_source_t source;   ///< Source of the event
    uint32_t timestamp;      ///< Timestamp when event was received
    union {
        button_event_t button;   ///< Button event data
        encoder_event_t encoder; ///< Encoder event data
        espnow_event_t espnow;   ///< ESPNOW event data
        touch_event_t touch;     ///< Touch event data
        switch_event_t switch_evt; ///< Switch event data
    } data;                     ///< Event payload
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
 * @param btn_q Button events queue
 * @param enc_q Encoder events queue
 * @param espnow_q ESPNOW events queue
 * @param touch_q Touch events queue
 * @param switch_q Switch events queue
 * @param int_q Integrated output queue
 * @return Initialized queue manager structure
 */
queue_manager_t init_queue_manager(QueueHandle_t btn_q, QueueHandle_t enc_q,
                                 QueueHandle_t espnow_q, QueueHandle_t touch_q,
                                 QueueHandle_t switch_q, QueueHandle_t int_q);

/**
 * @brief Main input integration task
 * @param pvParameters Pointer to queue manager structure
 */
void integrator_task(void *pvParameters);
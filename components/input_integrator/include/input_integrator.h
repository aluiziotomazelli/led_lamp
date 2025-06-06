


#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "button.h"
#include "encoder.h"

typedef struct {
    unsigned char mac_addr[6];
    unsigned char *data;
    int data_len;
} espnow_event_t;

typedef enum {
    EVENT_SOURCE_BUTTON,
    EVENT_SOURCE_ENCODER,
    EVENT_SOURCE_ESPNOW
} event_source_t;

typedef struct {
    event_source_t source;
    uint32_t timestamp;
    union {
        button_event_t button;
        encoder_event_t encoder;
        espnow_event_t espnow;
    } data;
} integrated_event_t;

typedef struct {
    QueueHandle_t button_queue;
    QueueHandle_t encoder_queue;
    QueueHandle_t espnow_queue;
    QueueHandle_t integrated_queue;
    QueueSetHandle_t queue_set;
} queue_manager_t;

queue_manager_t init_queue_manager(QueueHandle_t btn_q, QueueHandle_t enc_q, 
                                 QueueHandle_t espnow_q, QueueHandle_t int_q);

void integrator_task(void *pvParameters);
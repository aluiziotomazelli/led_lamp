#ifndef INPUT_EVENT_MANAGER_H
#define INPUT_EVENT_MANAGER_H

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

    union {
        button_event_t button;
        encoder_event_t encoder;
        espnow_event_t espnow;
    } data;

    // uint32_t timestamp;
} integrated_event_t;

#define BUTTON_FLAG   (1UL << 0)
#define ENCODER_FLAG  (1UL << 1)
#define ESPNOW_FLAG   (1UL << 2)

#define ALL_EVENT_FLAGS (BUTTON_FLAG | ENCODER_FLAG | ESPNOW_FLAG)

// Parameters for adapter tasks
// This struct is used to pass necessary handles to the adapter tasks.
typedef struct {
    QueueHandle_t component_queue;     // e.g., button_app_queue or encoder_event_queue
    TaskHandle_t  central_task_handle; // Handle of the central task to be notified
} adapter_task_params_t;

// Function prototypes for the adapter tasks
void button_adapter_task(void *pvParameters);
void encoder_adapter_task(void *pvParameters);
// void espnow_adapter_task(void *pvParameters); // Placeholder for future ESP-NOW adapter

#endif // INPUT_EVENT_MANAGER_H

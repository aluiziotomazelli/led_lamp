#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "driver/touch_pad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Event types for touch button
typedef enum {
    TOUCH_BUTTON_PRESS,
    TOUCH_BUTTON_RELEASE,
} touch_button_event_type_t;

// Struct for touch button events
typedef struct {
    touch_button_event_type_t type;
    touch_pad_t touch_pad;
} touch_button_event_t;

// Incomplete declaration to hide internal implementation
typedef struct touch_button_s touch_button_t;

// Configuration structure for touch button creation
typedef struct {
    touch_pad_t touch_pad;
    float threshold_percent; // e.g., 0.6 for 60%
} touch_button_config_t;

// Creates a new touch button instance.
touch_button_t *touch_button_create(const touch_button_config_t* config, QueueHandle_t output_queue);

// Deletes a touch button instance and frees resources.
void touch_button_delete(touch_button_t *btn);

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/touch_pad.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    TOUCH_NONE,
    TOUCH_PRESS,
    TOUCH_HOLD,
    TOUCH_ERROR
} touch_event_type_t;

typedef struct {
    touch_event_type_t type;
    touch_pad_t pad; // The touch pad number
} touch_event_t;

typedef struct {
    touch_pad_t pad;
    uint16_t threshold_percent; // Threshold relative to baseline
    uint16_t debounce_press_ms;
    uint16_t debounce_release_ms;
    uint16_t hold_time_ms;
    bool enable_hold_repeat;   // Se false, gera apenas um HOLD
    uint_fast16_t hold_repeat_interval_ms;
    uint16_t recalibration_interval_min;
} touch_config_t;

typedef struct touch_s touch_t;
touch_t* touch_create(const touch_config_t* config, QueueHandle_t output_queue);
void touch_delete(touch_t* touch_handle);


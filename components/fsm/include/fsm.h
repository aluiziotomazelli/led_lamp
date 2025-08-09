#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "input_integrator.h"


// FSM States
typedef enum {
    STATE_OFF,
    STATE_DISPLAY,
    STATE_EFFECT_CHANGE,
    STATE_EFFECT_SETUP,
    STATE_SYSTEM_SETUP
} fsm_state_t;

// FSM Action Types
typedef enum {
    ACTION_TURN_ON_LAMP,
    ACTION_TURN_OFF_LAMP,
    ACTION_ADJUST_BRIGHTNESS,
    ACTION_SELECT_EFFECT,
    ACTION_SAVE_EFFECT_PARAMETER,
    ACTION_SAVE_SYSTEM_PARAMETER,
    ACTION_REVERT_EFFECT_PARAMETERS,
    ACTION_CONFIRM_EFFECT,
    // Add more actions as needed
} fsm_action_type_t;

// FSM Output Action Structure
// Effect and Parameter Data Structures (from user's feedback)
typedef struct {
    char name[20];          
    int32_t value;          
    int32_t min;            
    int32_t max;            
    int32_t step;           
} effect_param_t;

typedef struct {
    fsm_action_type_t type;
    union {
        uint8_t brightness_value; // For ACTION_ADJUST_BRIGHTNESS
        uint8_t effect_id;        // For ACTION_SELECT_EFFECT
        effect_param_t effect_param;           // For ACTION_SAVE_EFFECT_PARAMETER
        // Add more action-specific data
    } data;
} fsm_output_action_t;



typedef struct {
    const char* name;       
    const uint8_t id;       
    const uint8_t param_count; 
    effect_param_t* params; 
} led_effect_t;

// Function declarations
void fsm_init(QueueHandle_t input_queue, QueueHandle_t output_queue);
void fsm_task(void *pvParameters);
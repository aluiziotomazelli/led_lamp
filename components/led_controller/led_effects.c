#include "led_effects.h"
#include <math.h>
#include <stdlib.h>

#include "hsv2rgb.h"



/* =========================== */
/* --- List of all effects --- */
/* =========================== */

// Include all effect headers
#include "effects/include/breathing.h"
#include "effects/include/candle.h"
#include "effects/include/candle_math.h"
#include "effects/include/christmas_tree.h"
#include "effects/include/christmas_twinkle.h"
#include "effects/include/random_twinkle.h"
#include "effects/include/static_color.h"
#include "effects/include/white_temp.h"

//
// Effect Definitions
//

effect_t effect_breathing = {
    .name = "Breathing",
    .run = run_breathing,
    .color_mode = COLOR_MODE_HSV,
    .params = params_breathing,
    .num_params = sizeof(params_breathing) / sizeof(effect_param_t),
    .is_dynamic = true
};

effect_t effect_candle = {
    .name = "Candle",
    .run = run_candle,
    .color_mode = COLOR_MODE_HSV,
    .params = params_candle,
    .num_params = sizeof(params_candle) / sizeof(effect_param_t),
    .is_dynamic = true
};

effect_t effect_candle_math = {
    .name = "Candle Math",
    .run = run_candle_math,
    .color_mode = COLOR_MODE_HSV,
    .params = params_candle_math,
    .num_params = sizeof(params_candle_math) / sizeof(effect_param_t),
    .is_dynamic = true
};

effect_t effect_christmas_tree = {
    .name = "Christmas",
    .run = run_christmas_tree,
    .color_mode = COLOR_MODE_HSV,
    .params = params_christmas_tree,
    .num_params = sizeof(params_christmas_tree) / sizeof(effect_param_t),
    .is_dynamic = true
};

effect_t effect_christmas_twinkle = {
    .name = "Christmas Twinkle",
    .run = run_christmas_twinkle,
    .color_mode = COLOR_MODE_RGB,
    .params = params_christmas_twinkle,
    .num_params = sizeof(params_christmas_twinkle) / sizeof(effect_param_t),
    .is_dynamic = true
};

effect_t effect_random_twinkle = {
    .name = "Random Twinkle",
    .run = run_random_twinkle,
    .color_mode = COLOR_MODE_HSV,
    .params = params_random_twinkle,
    .num_params = sizeof(params_random_twinkle) / sizeof(effect_param_t),
    .is_dynamic = true
};

effect_t effect_static_color = {
    .name = "Static Color",
    .run = run_static_color,
    .color_mode = COLOR_MODE_HSV,
    .params = params_static_color,
    .num_params = sizeof(params_static_color) / sizeof(effect_param_t),
    .is_dynamic = false
};

effect_t effect_white_temp = {
    .name = "White Temp",
    .run = run_white_temp,
    .color_mode = COLOR_MODE_RGB,
    .params = params_white_temp,
    .num_params = sizeof(params_white_temp) / sizeof(effect_param_t),
    .is_dynamic = false
};


//
// Master Effect List
//

effect_t *effects[] = {
    &effect_candle,
    &effect_white_temp,
    &effect_static_color,
    &effect_christmas_tree,
    &effect_candle_math,
    &effect_christmas_twinkle,
    &effect_random_twinkle,
    &effect_breathing // Added breathing here, was missing from original list
};

const uint8_t effects_count = sizeof(effects) / sizeof(effects[0]);

#ifndef LED_RENDERER_H
#define LED_RENDERER_H

#include "led_effects.h"
#include "esp_err.h"

esp_err_t led_renderer_init(void);
esp_err_t led_renderer_deinit(void);
esp_err_t led_renderer_update(const led_effect_t *effect, const effect_param_t *params, uint8_t brightness, bool is_on, uint32_t num_leds);

#endif // LED_RENDERER_H

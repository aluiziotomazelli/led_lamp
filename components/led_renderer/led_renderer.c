#include "led_renderer.h"
#include "led_controller.h"
#include "esp_log.h"
#include "project_config.h" // Assuming this will contain NUM_LEDS

static const char *TAG = "led_renderer";

esp_err_t led_renderer_init(void) {
    ESP_LOGI(TAG, "Initializing LED renderer");
    // For now, this is a stub.
    // If led_controller needs explicit initialization, it should be done elsewhere
    // or a dependency should be passed to this init function.
    return ESP_OK;
}

esp_err_t led_renderer_deinit(void) {
    ESP_LOGI(TAG, "De-initializing LED renderer");
    // For now, this is a stub.
    return ESP_OK;
}

esp_err_t led_renderer_update(const led_effect_t *effect, const effect_param_t *params, uint8_t brightness, bool is_on, uint32_t num_leds) {
    ESP_LOGD(TAG, "Updating LED renderer: effect_id=%d, brightness=%u, is_on=%d, num_leds=%lu",
             effect ? effect->id : -1, brightness, is_on, num_leds);

    // It's good practice to ensure num_leds does not exceed any known maximum like MAX_LEDS_PER_STRIP
    // This check might be better placed in led_controller or closer to project_config.h definitions
    if (num_leds == 0 || num_leds > MAX_LEDS_PER_STRIP) { // MAX_LEDS_PER_STRIP should be defined in project_config.h or led_controller.h
        ESP_LOGE(TAG, "Invalid num_leds: %lu. Must be > 0 and <= %d", num_leds, MAX_LEDS_PER_STRIP);
        // Optionally clear LEDs to a safe state if appropriate
        // led_controller_clear();
        // led_controller_refresh();
        return ESP_ERR_INVALID_ARG;
    }


    led_controller_set_brightness(brightness);

    if (!is_on) {
        ESP_LOGD(TAG, "LEDs are off, clearing.");
        led_controller_clear();
    } else {
        if (effect == NULL) {
            ESP_LOGE(TAG, "Effect is NULL but LEDs are on. Clearing LEDs.");
            led_controller_clear();
        } else if (effect->param_count > 0 && params == NULL) {
            ESP_LOGE(TAG, "Effect '%s' requires %d param(s), but params is NULL. Clearing LEDs.", effect->name, effect->param_count);
            led_controller_clear();
        } else {
            ESP_LOGD(TAG, "Applying effect: %s (ID: %d)", effect->name, effect->id);
            switch (effect->id) {
                case 0: // Static Color
                    if (effect->param_count >= 2) {
                        uint8_t hue = params[0].value;
                        uint8_t saturation = params[1].value;
                        ESP_LOGD(TAG, "Static Color: hue=%u, sat=%u", hue, saturation);
                        for (uint32_t i = 0; i < num_leds; i++) {
                            led_controller_set_pixel_hsv(i, hue, saturation, 255); // V is max, brightness controls overall intensity
                        }
                    } else {
                        ESP_LOGE(TAG, "Static Color effect requires 2 parameters (hue, saturation), but got %d. Clearing.", effect->param_count);
                        led_controller_clear();
                    }
                    break;
                case 1: // Candle
                    if (effect->param_count >= 1) {
                        uint8_t base_hue = params[0].value;
                        ESP_LOGD(TAG, "Candle: base_hue=%u", base_hue);
                        // Simplified: static color with fixed saturation
                        for (uint32_t i = 0; i < num_leds; i++) {
                            led_controller_set_pixel_hsv(i, base_hue, 220, 255); // Fixed saturation for candle-like color
                        }
                    } else {
                        ESP_LOGE(TAG, "Candle effect requires 1 parameter (base_hue), but got %d. Clearing.", effect->param_count);
                        led_controller_clear();
                    }
                    break;
                default:
                    ESP_LOGW(TAG, "Unimplemented effect ID: %d (%s). Clearing LEDs.", effect->id, effect->name);
                    led_controller_clear();
                    break;
            }
        }
    }

    led_controller_refresh();
    return ESP_OK;
}

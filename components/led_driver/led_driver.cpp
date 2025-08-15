#include "led_driver.h"
#include "esp_log.h"
#include "freertos/task.h"

// Includes from other components
#include "led_controller.h" // For led_strip_t
#include "project_config.h" // For hardware pin definitions

// FastLED include
#include <FastLED.h>

static const char *TAG = "LED_DRIVER";

// FastLED data array
static CRGB leds[NUM_LEDS];

// Queue for receiving pixel data from the controller
static QueueHandle_t q_pixels_in = NULL;

/**
 * @brief Configure the FastLED driver.
 */
static void configure_fastled(void) {
    ESP_LOGI(TAG, "Initializing FastLED driver");
    // The old driver used WS2812 with GRB format.
    FastLED.addLeds<WS2812, LED_STRIP_GPIO, GRB>(leds, NUM_LEDS);
    ESP_LOGI(TAG, "FastLED driver initialized successfully");
}

/**
 * @brief The main task for the LED driver.
 *
 * This task waits for new pixel data on the input queue and writes it to the
 * physical LED strip using FastLED.
 */
static void led_driver_task(void *pv) {
    led_strip_t strip_data;

    // Clear the strip on startup by showing black
    FastLED.clear();
    FastLED.show();
    ESP_LOGI(TAG, "Cleared strip on startup");

    while (1) {
        // Wait forever for new data to arrive
        if (xQueueReceive(q_pixels_in, &strip_data, portMAX_DELAY) == pdTRUE) {

            // The controller might send a null pointer if it's just a state update
            if (strip_data.pixels == NULL || strip_data.num_pixels == 0) {
                continue;
            }

            // Ensure we don't write past our buffer
            uint16_t pixels_to_update = (strip_data.num_pixels > NUM_LEDS) ? NUM_LEDS : strip_data.num_pixels;

            // Update the FastLED buffer pixel by pixel based on the color mode
            if (strip_data.mode == COLOR_MODE_HSV) {
                for (uint16_t i = 0; i < pixels_to_update; i++) {
                    hsv_t pixel_hsv = strip_data.pixels[i].hsv;
                    // FastLED's CHSV object takes H, S, V in 0-255 range.
                    // Our hsv_t has H in 0-359. We need to scale it.
                    uint8_t h = (pixel_hsv.h * 255) / 360;
                    leds[i] = CHSV(h, pixel_hsv.s, pixel_hsv.v);
                }
            } else { // It's RGB
                for (uint16_t i = 0; i < pixels_to_update; i++) {
                    rgb_t pixel_rgb = strip_data.pixels[i].rgb;
                    leds[i] = CRGB(pixel_rgb.r, pixel_rgb.g, pixel_rgb.b);
                }
            }

            // Refresh the strip to show the new colors
            FastLED.show();
        }
    }
}

/**
 * @brief Public function to initialize the LED driver component.
 *
 * This function is wrapped in extern "C" to allow it to be called from the
 * C code in main.c
 */
extern "C" void led_driver_init(QueueHandle_t input_queue) {
    if (input_queue == NULL) {
        ESP_LOGE(TAG, "Input queue is NULL. Cannot initialize.");
        return;
    }
    q_pixels_in = input_queue;

    // Configure the hardware for the LED strip
    configure_fastled();

    // Create the driver task
    BaseType_t task_created =
        xTaskCreate(led_driver_task, "LED_DRV_T", LED_DRIVER_TASK_STACK_SIZE,
                    NULL, LED_DRIVER_TASK_PRIORITY, NULL);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED driver task");
    } else {
        ESP_LOGI(TAG, "LED driver task created successfully");
    }
}

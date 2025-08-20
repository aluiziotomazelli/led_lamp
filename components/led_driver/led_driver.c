/**
 * @file led_driver.c
 * @brief LED Strip Driver implementation with color correction
 * 
 * @details This file implements the LED strip driver that receives pixel data
 *          from a queue, applies color correction, and sends it to physical LEDs.
 *          Supports both RGB and HSV color modes with hardware acceleration.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <stdint.h>
#include "driver/gpio.h"

// Set log level for this module, must come before esp_log.h
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
// ESP-IDF system services
#include "esp_log.h"
#include "esp_err.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Project specific headers
#include "led_driver.h"
#include "led_controller.h"   // For led_strip_t
#include "project_config.h"   // For hardware pin definitions

// External components
#include "hsv2rgb.h"
#include "led_strip.h"

static const char *TAG = "LED_DRIVER";

//------------------------------------------------------------------------------
// PRIVATE VARIABLES
//------------------------------------------------------------------------------

/// @brief Handle for the LED strip hardware interface
static led_strip_handle_t led_strip_handle;

/// @brief Queue for receiving pixel data from the controller
static QueueHandle_t q_pixels_in = NULL;

/// @brief Mutex to protect access to the led_strip_handle
static SemaphoreHandle_t led_strip_mutex = NULL;

/// @brief Global color correction values for white balance
static uint16_t g_correction_r = 255;  ///< Red channel correction
static uint16_t g_correction_g = 200;  ///< Green channel correction  
static uint16_t g_correction_b = 140;  ///< Blue channel correction

//------------------------------------------------------------------------------
// PRIVATE FUNCTION DECLARATIONS
//------------------------------------------------------------------------------

/**
 * @brief Configure the LED strip hardware
 * 
 * @return `ESP_OK` on success, or an error code on failure
 * 
 * @note Sets up GPIO, LED model, and SPI backend for WS2812 strip
 */
static esp_err_t configure_led_strip(void);

/**
 * @brief Main task for the LED driver
 * 
 * @param pv Task parameters (unused)
 * 
 * @note This task waits for pixel data on the input queue and writes to LEDs
 */
static void led_driver_task(void *pv);

//------------------------------------------------------------------------------
// PRIVATE FUNCTION IMPLEMENTATIONS
//------------------------------------------------------------------------------

/**
 * @brief Configure the LED strip driver hardware
 */
static esp_err_t configure_led_strip(void) {
    ESP_LOGI(TAG, "Initializing LED strip");

    // General configuration for the LED strip
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = NUM_LEDS,
        .led_model = LED_MODEL_WS2812,
        // My strip is GRB format
        .color_component_format = {
            .format = {
                .r_pos = 1,  // red is the second byte in the color data
                .g_pos = 0,  // green is the first byte in the color data  
                .b_pos = 2,  // blue is the third byte in the color data
                .num_components = 3,  // total 3 color components
            },
        },
        .flags = {
            .invert_out = false,  // don't invert the output signal
        }
    };

    // Backend configuration for SPI
    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT,
        .spi_bus = LED_STRIP_SPI_HOST,
        .flags = {
            .with_dma = true,  // Use DMA for efficient data transfer
        }
    };

    // Create the LED strip object
    esp_err_t err = led_strip_new_spi_device(&strip_config, &spi_config, &led_strip_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create SPI LED strip object: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "LED strip object created successfully");
    return ESP_OK;
}

/**
 * @brief Main task for processing LED data
 */
static void led_driver_task(void *pv) {
    led_strip_t strip_data;

    // Clear the strip on startup
    ESP_LOGI(TAG, "Clearing strip on startup");
    esp_err_t err = led_strip_clear(led_strip_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear LED strip: %s", esp_err_to_name(err));
    }

    while (1) {
        // Wait forever for new data to arrive
        if (xQueueReceive(q_pixels_in, &strip_data, portMAX_DELAY) == pdTRUE) {

            // The controller might send a null pointer if it's just a state update
            if (strip_data.pixels == NULL || strip_data.num_pixels == 0) {
                continue;
            }

            if (xSemaphoreTake(led_strip_mutex, portMAX_DELAY) == pdTRUE) {
                // Loop through all pixels, apply color correction, and set them
                for (uint16_t i = 0; i < strip_data.num_pixels; i++) {
                    rgb_t final_rgb;

                    // Convert HSV to RGB if needed
                    if (strip_data.mode == COLOR_MODE_HSV) {
                        hsv_t hsv = strip_data.pixels[i].hsv;
                        hsv_to_rgb_spectrum_deg(hsv.h, hsv.s, hsv.v, &final_rgb.r,
                                                &final_rgb.g, &final_rgb.b);
                    } else {
                        final_rgb = strip_data.pixels[i].rgb;
                    }

                    // Apply color correction to each channel
                    final_rgb.r = (uint8_t)(((uint16_t)final_rgb.r * g_correction_r) >> 8);
                    final_rgb.g = (uint8_t)(((uint16_t)final_rgb.g * g_correction_g) >> 8);
                    final_rgb.b = (uint8_t)(((uint16_t)final_rgb.b * g_correction_b) >> 8);

                    // Set the pixel color on the strip
                    err = led_strip_set_pixel(led_strip_handle, i, final_rgb.r,
                                            final_rgb.g, final_rgb.b);
                    if (err != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to set pixel %d: %s", i, esp_err_to_name(err));
                    }
                }

                // Refresh the strip to show the new colors
                err = led_strip_refresh(led_strip_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(err));
                }
                xSemaphoreGive(led_strip_mutex);
            }
        }
    }
}

//------------------------------------------------------------------------------
// PUBLIC FUNCTION IMPLEMENTATIONS  
//------------------------------------------------------------------------------

/**
 * @brief Set global color correction values
 */
void led_driver_set_correction(uint8_t r, uint8_t g, uint8_t b) {
    g_correction_r = r;
    g_correction_g = g;
    g_correction_b = b;
    ESP_LOGI(TAG, "Set color correction to R:%d, G:%d, B:%d", r, g, b);
}

/**
 * @brief Initialize the LED driver component
 */
void led_driver_init(QueueHandle_t input_queue) {
    if (input_queue == NULL) {
        ESP_LOGE(TAG, "Input queue is NULL. Cannot initialize.");
        return;
    }
    q_pixels_in = input_queue;

    // Create the mutex for thread-safe access to the LED strip
    led_strip_mutex = xSemaphoreCreateMutex();
    if (led_strip_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create LED strip mutex. Aborting init.");
        return;
    }

    // Configure the hardware for the LED strip
    esp_err_t config_status = configure_led_strip();
    if (config_status != ESP_OK) {
        ESP_LOGE(TAG, "LED strip configuration failed. Aborting init.");
        // Depending on desired robustness, you might want to handle this differently
        return;
    }

    // Create the driver task
    BaseType_t task_created = xTaskCreate(led_driver_task, "LED_DRV_T", 
                                        LED_DRIVER_TASK_STACK_SIZE, NULL, 
                                        LED_DRIVER_TASK_PRIORITY, NULL);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LED driver task");
    } else {
        ESP_LOGI(TAG, "LED driver task created successfully");
    }
}

/**
 * @brief Prepares the LED driver for system sleep.
 */
void led_driver_prepare_for_sleep(void) {
    if (led_strip_handle && xSemaphoreTake(led_strip_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Clear all pixels to black
        esp_err_t err = led_strip_clear(led_strip_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to clear strip for sleep: %s", esp_err_to_name(err));
        }
        // The clear command only stages the changes. A refresh is needed
        // to write the cleared state to the physical strip.
        err = led_strip_refresh(led_strip_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to refresh strip for sleep: %s", esp_err_to_name(err));
            xSemaphoreGive(led_strip_mutex); // Release mutex on error
            return; // Don't try to hold the pin if refresh failed
        }

        // Latch the GPIO pin's current state (low) so it is held during sleep
        err = gpio_hold_en(LED_STRIP_GPIO);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to hold GPIO %d: %s", LED_STRIP_GPIO, esp_err_to_name(err));
        }

        xSemaphoreGive(led_strip_mutex);
    } else {
        ESP_LOGE(TAG, "Could not obtain LED strip mutex to prepare for sleep.");
    }
}
#include "led_driver.h"
#include "esp_log.h"
#include "freertos/task.h"

// Includes from other components
#include "led_controller.h" // For led_strip_t
#include "led_effects.h"
#include "project_config.h" // For hardware pin definitions

// Includes from IDF and components
#include "esp_err.h"
#include "led_strip.h"

static const char *TAG = "LED_DRIVER";

// Handle for the LED strip
static led_strip_handle_t led_strip_handle;

// Queue for receiving pixel data from the controller
static QueueHandle_t q_pixels_in = NULL;

/**
 * @brief Configure the LED strip driver.
 *
 * Sets up the GPIO, LED model, and SPI backend for the WS2812 strip.
 *
 * @return `ESP_OK` on success, or an error code on failure.
 */
static esp_err_t configure_led_strip(void) {
	ESP_LOGI(TAG, "Initializing LED strip");

	// General configuration for the LED strip
	led_strip_config_t strip_config = {
		.strip_gpio_num = LED_STRIP_GPIO,
		.max_leds = NUM_LEDS,
		.led_model = LED_MODEL_WS2812,
		.color_component_format =
			LED_STRIP_COLOR_COMPONENT_FMT_GRB, // My strip is GRB
	};

	// Backend configuration for SPI
	led_strip_spi_config_t spi_config = {.clk_src = SPI_CLK_SRC_DEFAULT,
										 .spi_bus = LED_STRIP_SPI_HOST,
										 .flags = {
											 .with_dma = true,
										 }};

	// Create the LED strip object
	esp_err_t err =
		led_strip_new_spi_device(&strip_config, &spi_config, &led_strip_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to create SPI LED strip object: %s",
				 esp_err_to_name(err));
		return err;
	}

	ESP_LOGI(TAG, "LED strip object created successfully");
	return ESP_OK;
}

/**
 * @brief The main task for the LED driver.
 *
 * This task waits for new pixel data on the input queue and writes it to the
 * physical LED strip.
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

			// The controller might send a null pointer if it's just a state
			// update
			if ((strip_data.pixels == NULL) ||
				strip_data.num_pixels == 0) {
				continue;
			}

			// Update the strip pixel by pixel
			for (uint16_t i = 0; i < strip_data.num_pixels; i++) {
				if (strip_data.is_hsv) {
					hsv_t hsv = strip_data.pixels[i].hsv;
					err = led_strip_set_pixel_hsv(led_strip_handle, i, hsv.h,
												  hsv.s, hsv.v);
//						ESP_LOGI(TAG, "HSV, h = %d, s = %d, v = %d", hsv.h, hsv.s, hsv.v);
					if (err != ESP_OK) {
						ESP_LOGE(TAG, "Failed to set pixel %d: %s", i,
								 esp_err_to_name(err));
					}
				} else {
					rgb_t rgb = strip_data.pixels[i].rgb;
					err = led_strip_set_pixel(led_strip_handle, i, rgb.r,
											  rgb.g, rgb.b);
//						ESP_LOGI(TAG, "RGB, R = %d, G = %d, B = %d", rgb.r, rgb.g, rgb.b);
					if (err != ESP_OK) {
						ESP_LOGE(TAG, "Failed to set pixel %d: %s", i,
								 esp_err_to_name(err));
					}
				}
			}

			// Refresh the strip to show the new colors
			err = led_strip_refresh(led_strip_handle);
			if (err != ESP_OK) {
				ESP_LOGE(TAG, "Failed to refresh LED strip: %s",
						 esp_err_to_name(err));
			}
		}
	}
}

/**
 * @brief Public function to initialize the LED driver component.
 */
void led_driver_init(QueueHandle_t input_queue) {
	if (input_queue == NULL) {
		ESP_LOGE(TAG, "Input queue is NULL. Cannot initialize.");
		return;
	}
	q_pixels_in = input_queue;

	// Configure the hardware for the LED strip
	esp_err_t config_status = configure_led_strip();
	if (config_status != ESP_OK) {
		ESP_LOGE(TAG, "LED strip configuration failed. Aborting init.");
		// Depending on desired robustness, you might want to handle this
		// differently
		return;
	}

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

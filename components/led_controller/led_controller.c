#include "led_controller.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "led_strip_spi.h"
#include "led_strip_types.h"
#include <inttypes.h>

// Static variables
static const char *TAG = "led_controller";
static led_strip_handle_t s_led_strip_handle = NULL;
static led_controller_config_t s_current_config;
static uint8_t s_global_brightness = 128;
static spi_host_device_t s_configured_spi_host = -1;
static bool s_spi_bus_was_initialized_by_us = false;

esp_err_t led_controller_init(const led_controller_config_t *config) {
	ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG,
						"Configuration cannot be null");
	ESP_RETURN_ON_FALSE(config->num_leds > 0, ESP_ERR_INVALID_ARG, TAG,
						"Number of LEDs must be > 0");

	if (s_led_strip_handle) {
		ESP_LOGW(TAG,
				 "LED controller already initialized. Deinitializing first.");
		led_controller_deinit();
	}

	s_current_config = *config;
	s_configured_spi_host = config->spi_host;

//	spi_bus_config_t buscfg = {
//		.mosi_io_num = config->spi_mosi_gpio,
//		.sclk_io_num = config->spi_sclk_gpio,
//		.miso_io_num = -1,
//		.quadwp_io_num = -1,
//		.quadhd_io_num = -1,
//		.max_transfer_sz = config->num_leds * 4, // 4 bytes per pixel for safety
//	};

//	ESP_LOGI(TAG, "Initializing SPI bus %d...", (int)config->spi_host);
//	esp_err_t ret =
//		spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO);
//	if (ret == ESP_ERR_INVALID_STATE) {
//		ESP_LOGW(TAG, "SPI bus %d already initialized.", (int)config->spi_host);
//		s_spi_bus_was_initialized_by_us = false;
//		// Continue, assuming the bus was initialized with a compatible
//		// configuration
//	} else if (ret == ESP_OK) {
//		s_spi_bus_was_initialized_by_us = true;
//		ESP_LOGI(TAG, "SPI bus %d initialized successfully.",
//				 (int)config->spi_host);
//	} else {
//		ESP_LOGE(TAG, "Failed to initialize SPI bus %d: %s",
//				 (int)config->spi_host, esp_err_to_name(ret));
//		s_configured_spi_host = -1;
//		return ret;
//	}

	led_strip_config_t strip_config = {
		.strip_gpio_num = config->data_gpio,
		.max_leds = config->num_leds,
		.led_model = config->model,
		.color_component_format = config->pixel_format,
		.flags.invert_out = config->invert,
	};

	led_strip_spi_config_t spi_config = {
		.clk_src = config->spi_clk_src,
		.spi_bus = config->spi_host,
		.flags.with_dma = config->with_dma,
	};

	ESP_LOGI(TAG, "Creating SPI LED strip object...");
	ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config,
											 &s_led_strip_handle));
	ESP_LOGI(TAG, "SPI LED strip object created.");

	s_global_brightness = 128;
	return led_strip_clear(s_led_strip_handle);
}

esp_err_t led_controller_set_pixel_hsv(uint32_t index, uint16_t h, uint8_t s,
									   uint8_t v) {
	ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG,
						"LED strip not initialized");
	ESP_RETURN_ON_FALSE(index < s_current_config.num_leds, ESP_ERR_INVALID_ARG,
						TAG, "Pixel index %" PRIu32 " out of bounds", index);
	uint8_t final_v = ((uint16_t)v * s_global_brightness) / 255;
	return led_strip_set_pixel_hsv(s_led_strip_handle, index, h, s, final_v);
}

esp_err_t led_controller_set_brightness(uint8_t brightness) {
	ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG,
						"LED strip not initialized");
	s_global_brightness = brightness;
	return ESP_OK;
}

esp_err_t led_controller_set_power(bool on) {
	ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG,
						"LED strip not initialized");
	if (!on) {
		return led_controller_clear();
	}
	return ESP_OK;
}

esp_err_t led_controller_clear(void) {
	ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG,
						"LED strip not initialized");
	return led_strip_clear(s_led_strip_handle);
}

esp_err_t led_controller_refresh(void) {
	ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG,
						"LED strip not initialized");
	return led_strip_refresh(s_led_strip_handle);
}

esp_err_t led_controller_deinit(void) {
	if (s_led_strip_handle) {
		led_strip_del(s_led_strip_handle);
		s_led_strip_handle = NULL;
	}
	if (s_spi_bus_was_initialized_by_us && s_configured_spi_host != -1) {
		spi_bus_free(s_configured_spi_host);
		s_spi_bus_was_initialized_by_us = false;
		s_configured_spi_host = -1;
	}
	ESP_LOGI(TAG, "LED controller deinitialized.");
	return ESP_OK;
}

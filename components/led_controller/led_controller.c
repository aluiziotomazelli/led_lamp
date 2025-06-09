#include "led_controller.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_check.h" // For ESP_RETURN_ON_ERROR
#include "driver/spi_common.h" // For SPI_DMA_CH_AUTO and spi_common_gpio_setup

// Static variables
static const char *TAG = "led_controller";
static led_strip_handle_t s_led_strip_handle = NULL;
static led_controller_config_t s_current_config; // To store a copy of the config for internal use (e.g. num_leds)
static uint8_t s_global_brightness = 128; // Default brightness

static spi_host_device_t s_configured_spi_host = -1; // Unconfigured SPI host
static bool s_spi_bus_was_initialized_by_us = false;

// Note: The original helper functions led_controller_spi_bus_init and led_controller_spi_bus_deinit
// are now effectively integrated into led_controller_init and led_controller_deinit respectively.

esp_err_t led_controller_init(const led_controller_config_t *config) {
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Configuration cannot be null");
    ESP_RETURN_ON_FALSE(config->num_leds > 0, ESP_ERR_INVALID_ARG, TAG, "Number of LEDs must be > 0");
    ESP_RETURN_ON_FALSE(config->spi_mosi_gpio >= 0, ESP_ERR_INVALID_ARG, TAG, "MOSI GPIO must be valid");
    ESP_RETURN_ON_FALSE(config->spi_sclk_gpio >= 0, ESP_ERR_INVALID_ARG, TAG, "SCLK GPIO must be valid");

    esp_err_t ret = ESP_OK;

    // If already initialized, deinitialize first to apply new config or ensure clean state
    if (s_led_strip_handle) {
        ESP_LOGW(TAG, "LED controller already initialized. Deinitializing first.");
        led_controller_deinit(); // This will also attempt to free SPI bus if we initialized it.
    }

    s_current_config = *config; // Store a copy of the configuration
    s_configured_spi_host = config->spi_host; // Store which SPI host we are using

    // --- SPI Bus Configuration ---
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->spi_mosi_gpio,
        .miso_io_num = -1, // Not used for LED strips
        .sclk_io_num = config->spi_sclk_gpio,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        // Max transfer size for SK6812/WS2812 (RGB) is 3 bytes per LED.
        // Add some overhead for SPI command/reset data (e.g., 8-12 bytes for SK6812 reset pulse).
        // Using a fixed reasonable size or calculating more precisely.
        // led_strip driver might also have its own max transfer size definition.
        // For now, let's use a common default or calculate based on num_leds.
        // IDF's led_strip uses `num_leds * 3` for SK6812.
        .max_transfer_sz = config->num_leds * 3 + 8, // Example: 3 bytes/LED + 8 byte reset margin
        .flags = 0, // Default flags
        .intr_flags = 0
    };

    ESP_LOGI(TAG, "Initializing SPI bus %d for LED strip...", config->spi_host);
    ret = spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK) {
        s_spi_bus_was_initialized_by_us = true;
        ESP_LOGI(TAG, "SPI bus %d initialized successfully by LED controller.", config->spi_host);
    } else if (ret == ESP_ERR_INVALID_STATE) {
        s_spi_bus_was_initialized_by_us = false;
        ESP_LOGW(TAG, "SPI bus %d was already initialized by another driver.", config->spi_host);
        ret = ESP_OK; // Treat as success for our component's use, assuming compatible configuration
    } else {
        ESP_LOGE(TAG, "Failed to initialize SPI bus %d: %s", config->spi_host, esp_err_to_name(ret));
        s_configured_spi_host = -1; // Mark as unconfigured on failure
        return ret; // Propagate the error
    }

    // --- LED Strip Hardware Configuration (SPI Backend) ---
    led_strip_spi_config_t spi_strip_cfg = { // Renamed to avoid conflict with outer 'spi_cfg' if any
        .clk_src = config->spi_clk_src,
        .flags = 0, // Default flags. Consider LED_STRIP_SPI_FLAG_WITH_DMA if issues or for performance.
                     // The underlying driver has `flags.with_dma` in its main config.
                     // This .flags here is for specific SPI characteristics if any.
        .clock_speed_hz = config->clk_speed_hz, // Corrected field name from plan
    };

    // --- LED Strip Driver Configuration ---
    led_strip_config_t strip_driver_cfg = { // Renamed to avoid conflict
        .strip_gpio_num = -1, // For SPI, specific data pin is MOSI, not a single strip_gpio_num.
                              // The `led_strip` component for SPI uses MOSI from `spi_bus_config_t`.
                              // Setting to -1 is typical for SPI mode as per ESP-IDF examples for `led_strip_new_spi_device`.
        .max_leds = config->num_leds,
        .led_pixel_format = config->pixel_format,
        .led_model = config->model,
        .flags.invert_out = false, // Typically false
    };

    // --- Create LED Strip Driver Instance ---
    ESP_LOGI(TAG, "Creating SPI LED strip driver instance...");
    esp_err_t strip_init_ret = led_strip_new_spi_device(&strip_driver_cfg, &spi_strip_cfg, &s_led_strip_handle);
    if (strip_init_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create SPI LED strip driver: %s", esp_err_to_name(strip_init_ret));
        if (s_spi_bus_was_initialized_by_us) {
            ESP_LOGI(TAG, "Freeing SPI bus %d due to led_strip_new_spi_device failure.", s_configured_spi_host);
            spi_bus_free(s_configured_spi_host);
            s_spi_bus_was_initialized_by_us = false;
        }
        s_configured_spi_host = -1;
        s_led_strip_handle = NULL; // Ensure handle is NULL on failure
        return strip_init_ret; // Return the specific error from strip creation
    }
    ESP_LOGI(TAG, "SPI LED strip driver created successfully.");

    // Set initial brightness
    s_global_brightness = 128; // Default brightness, can be changed by set_brightness
    esp_err_t brightness_ret = led_strip_set_brightness(s_led_strip_handle, s_global_brightness);
    if (brightness_ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set initial brightness: %s", esp_err_to_name(brightness_ret));
        // Not treating as fatal for init, but logging it. Could goto cleanup.
        // Let's make it fatal to ensure consistent state or proper cleanup.
        goto err_strip_created;
    }

    ESP_LOGI(TAG, "LED controller initialized successfully.");
    return ESP_OK;

// Error handling jump point if operations after strip creation fail
err_strip_created:
    if (s_led_strip_handle) {
        led_strip_del(s_led_strip_handle);
        s_led_strip_handle = NULL;
    }
    if (s_spi_bus_was_initialized_by_us) {
        spi_bus_free(s_configured_spi_host);
        s_spi_bus_was_initialized_by_us = false;
    }
    s_configured_spi_host = -1;
    return brightness_ret; // Return the error that caused the jump
}

esp_err_t led_controller_set_pixel_hsv(uint32_t index, uint16_t h, uint8_t s, uint8_t v) {
    ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG, "LED strip not initialized");
    ESP_RETURN_ON_FALSE(index < s_current_config.num_leds, ESP_ERR_INVALID_ARG, TAG, "Pixel index out of bounds");

    // The led_strip component handles HSV to RGB conversion internally
    return led_strip_set_pixel_hsv(s_led_strip_handle, index, h, s, v);
}

esp_err_t led_controller_set_brightness(uint8_t brightness) {
    ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG, "LED strip not initialized");
    s_global_brightness = brightness;
    return led_strip_set_brightness(s_led_strip_handle, s_global_brightness);
}

esp_err_t led_controller_set_power(bool on) {
    ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG, "LED strip not initialized");
    // Note: Power state is conceptual for this controller. If 'on' is false, FSM usually calls clear.
    // This function mainly serves to inform the system or to trigger a clear if turning off.
    if (!on) {
        ESP_LOGI(TAG, "LED power set to OFF. Clearing strip via led_controller_clear().");
        return led_controller_clear(); // This clears the buffer. Refresh needed separately.
    } else {
        ESP_LOGI(TAG, "LED power set to ON. Pixels will update on next refresh cycle.");
    }
    return ESP_OK;
}

esp_err_t led_controller_clear(void) {
    ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG, "LED strip not initialized");
    esp_err_t ret = led_strip_clear(s_led_strip_handle);
    ESP_RETURN_ON_ERROR(ret, TAG, "Failed to clear strip");
    // Note: The original plan mentioned potentially calling refresh here.
    // However, the standard behavior for led_strip_clear is to only modify the buffer.
    // A separate call to led_strip_refresh (or led_controller_refresh) is required
    // to make the cleared strip visible. This provides more control to the caller.
    return ret;
}

esp_err_t led_controller_refresh(void) {
    ESP_RETURN_ON_FALSE(s_led_strip_handle, ESP_ERR_INVALID_STATE, TAG, "LED strip not initialized");
    return led_strip_refresh(s_led_strip_handle);
}

esp_err_t led_controller_deinit(void) {
    if (s_led_strip_handle) {
        esp_err_t ret = led_strip_del(s_led_strip_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to delete LED strip: %s", esp_err_to_name(ret));
            // Continue trying to deinitialize other parts if possible
        }
        s_led_strip_handle = NULL;
    }

    // Deinitialize SPI bus if it was initialized by this component
    if (s_spi_bus_was_initialized_by_us && s_configured_spi_host != -1) {
        ESP_LOGI(TAG, "Freeing SPI bus %d as it was initialized by us.", s_configured_spi_host);
        esp_err_t free_ret = spi_bus_free(s_configured_spi_host);
        if (free_ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to free SPI bus %d: %s", s_configured_spi_host, esp_err_to_name(free_ret));
            // Don't return error from deinit if bus free fails, but log it.
        }
        s_spi_bus_was_initialized_by_us = false;
    }
    s_configured_spi_host = -1; // Mark as unconfigured

    ESP_LOGI(TAG, "LED controller deinitialized.");
    return ESP_OK;
}

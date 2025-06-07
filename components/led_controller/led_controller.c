#include "led_controller.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "esp_check.h" // For ESP_RETURN_ON_ERROR

// Static variables
static const char *TAG = "led_controller";
static led_strip_handle_t s_led_strip_handle = NULL;
static led_controller_config_t s_current_config;
static uint8_t s_global_brightness = 128; // Default brightness

// Helper function to initialize SPI bus if not already configured
static esp_err_t led_controller_spi_bus_init(const led_controller_config_t *config) {
    esp_err_t ret;

    spi_bus_config_t buscfg = {
        .mosi_io_num = config->spi_mosi_gpio,
        .sclk_io_num = config->spi_sclk_gpio,
        .miso_io_num = -1, // Not used for LED strips
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = config->num_leds * 3 + 8 // Max data size for SK6812 (3 bytes per LED + Treset)
    };

    // Initialize the SPI bus
    // Check if the bus is already initialized by another peripheral with compatible settings.
    // For simplicity, we'll try to initialize it. If it fails with ESP_ERR_INVALID_STATE,
    // it means it's already initialized, which might be okay if settings are compatible.
    // A more robust solution would involve a shared SPI bus management component.
    ret = spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus (host: %d) already initialized.", config->spi_host);
        // Potentially check if existing config is compatible, for now, assume it is.
        ret = ESP_OK;
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "SPI bus initialization failed");
    return ret;
}

// Helper function to deinitialize SPI bus
static esp_err_t led_controller_spi_bus_deinit(spi_host_device_t spi_host) {
    // For now, assume the bus might be shared, so don't deinitialize if other devices might be using it.
    // A more robust system would track shared resources.
    // esp_err_t ret = spi_bus_free(spi_host);
    // ESP_RETURN_ON_ERROR(ret, TAG, "SPI bus deinitialization failed");
    ESP_LOGI(TAG, "SPI bus deinitialization skipped (assumed shared or managed elsewhere).");
    return ESP_OK;
}

esp_err_t led_controller_init(const led_controller_config_t *config) {
    ESP_RETURN_ON_FALSE(config, ESP_ERR_INVALID_ARG, TAG, "Configuration cannot be null");
    ESP_RETURN_ON_FALSE(config->num_leds > 0, ESP_ERR_INVALID_ARG, TAG, "Number of LEDs must be > 0");

    esp_err_t ret = ESP_OK;

    // Store configuration
    s_current_config = *config;

    // Note on SPI Bus Initialization:
    // The underlying 'led_strip' component using an SPI backend requires the SPI bus
    // to be initialized beforehand (e.g., by calling spi_bus_initialize).
    // This led_controller component assumes that the SPI bus (specified in config->spi_host)
    // has been initialized by another part of the application or a dedicated SPI management component.
    // Helper functions (led_controller_spi_bus_init/deinit) are provided but commented out
    // as their usage depends on the overall system's SPI resource management strategy.

    ESP_LOGI(TAG, "Initializing LED strip with SPI host %d, MOSI:%d, SCLK:%d, %" PRIu32 " LEDs, CLK %" PRIu32 "Hz",
             config->spi_host, config->spi_mosi_gpio, config->spi_sclk_gpio, config->num_leds, config->clk_speed_hz);


    // Configuration for the SPI specific part of the LED strip driver
    led_strip_spi_config_t spi_cfg = {
        .clk_src = SPI_CLK_SRC_DEFAULT,       // Use default SPI clock source
        .flags.with_dma = true,               // Use DMA for SPI transactions (preferred for performance)
        .spi_bus = config->spi_host,          // SPI bus to use
    };

    // General configuration for the LED strip driver
    led_strip_config_t strip_config = {
        .strip_gpio_num = config->spi_mosi_gpio, // In SPI mode, this is the MOSI pin.
        .max_leds = config->num_leds,            // Maximum number of LEDs in the strip.
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of the LEDs (GRB is common for SK6812/WS2812).
        .led_model = LED_MODEL_SK6812,            // LED model (e.g., SK6812, WS2812). Check your specific LED strip.
        .flags.invert_out = false,                // Set to true if the output signal needs to be inverted (rare).
    };

    ESP_LOGI(TAG, "Attempting to create new SPI LED strip device...");
    ret = led_strip_new_spi_device(&strip_config, &spi_cfg, &s_led_strip_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "led_strip_new_spi_device failed: %s (0x%x)", esp_err_to_name(ret), ret);
        // If it failed, deinit SPI bus if we initialized it.
        // led_controller_spi_bus_deinit(config->spi_host);
        return ret;
    }
     ESP_RETURN_ON_ERROR(ret, TAG, "led_strip_new_spi_device failed");


    // Set initial brightness
    s_global_brightness = 128; // Default brightness
    ret = led_strip_set_brightness(s_led_strip_handle, s_global_brightness);
    ESP_GOTO_ON_ERROR(ret, err_led_strip, TAG, "Failed to set initial brightness");

    ESP_LOGI(TAG, "LED controller initialized successfully.");
    return ESP_OK;

err_led_strip:
    if (s_led_strip_handle) {
        led_strip_del(s_led_strip_handle);
        s_led_strip_handle = NULL;
    }
    // led_controller_spi_bus_deinit(config->spi_host); // If we initialized it
    return ret;
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
    esp_err_t ret;
    if (!on) {
        ret = led_strip_clear(s_led_strip_handle);
        ESP_RETURN_ON_ERROR(ret, TAG, "Failed to clear strip for power off");
        // No need to call refresh here, clear implies all off. Refresh will happen if user wants to update.
    } else {
        // Powering on doesn't necessarily mean doing anything to the pixels immediately.
        // It just means subsequent calls to set_pixel and refresh will work.
        // If there was a previous state, it could be restored here, but that's more complex.
        ESP_LOGI(TAG, "LED power set to ON. Pixels will update on next refresh.");
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
    // ret = led_controller_spi_bus_deinit(s_current_config.spi_host);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to deinitialize SPI bus: %s", esp_err_to_name(ret));
    // }

    ESP_LOGI(TAG, "LED controller deinitialized.");
    return ESP_OK;
}

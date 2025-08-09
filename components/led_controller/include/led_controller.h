#pragma once

#include "esp_err.h"
#include "led_strip.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Configuration structure for the LED controller.
 * This structure holds all necessary parameters to initialize and operate an SPI-based LED strip.
 */
typedef struct {
    spi_host_device_t spi_host; ///< SPI host device (e.g., SPI2_HOST, SPI3_HOST). The SPI bus should be initialized before calling `led_controller_init`.
    uint32_t clk_speed_hz;      ///< SPI clock speed in Hertz. Common values are 10MHz (10 * 1000 * 1000).
    uint32_t num_leds;          ///< Number of LEDs in the strip. This defines the size of the buffer.
    uint8_t spi_mosi_gpio;      ///< GPIO number used for the SPI MOSI (Data) signal.
    uint8_t spi_sclk_gpio;      ///< GPIO number used for the SPI SCLK (Clock) signal.
   led_color_component_format_t  pixel_format; ///< Pixel format of the LED strip (e.g., LED_PIXEL_FORMAT_GRB).
    led_model_t model;         ///< Model of the LED strip (e.g., LED_MODEL_SK6812, LED_MODEL_WS2812).
    spi_clock_source_t spi_clk_src;  ///< SPI clock source. Use from `spi_common.h` (e.g., SPI_CLK_SRC_DEFAULT).
} led_controller_config_t;

/**
 * @brief Initializes the LED controller and the underlying LED strip.
 *
 * Configures the LED strip driver with the provided parameters.
 * The SPI bus associated with `config->spi_host` must be initialized before calling this function.
 *
 * @param config Pointer to a `led_controller_config_t` structure containing initialization parameters.
 *               This structure must not be NULL and must contain valid configuration values.
 * @return
 *      - ESP_OK: If the LED controller and strip were initialized successfully.
 *      - ESP_ERR_INVALID_ARG: If `config` is NULL or contains invalid parameters (e.g., num_leds is 0).
 *      - ESP_ERR_NO_MEM: If memory allocation for LED strip resources failed.
 *      - Other ESP_ERR_*: Errors from the underlying `led_strip` component.
 */
esp_err_t led_controller_init(const led_controller_config_t *config);

/**
 * @brief Sets the color of a single pixel on the LED strip using HSV color model.
 *
 * The change is made to an internal buffer. `led_controller_refresh()` must be called
 * to update the physical LED strip.
 *
 * @param index The zero-based index of the pixel to set (0 to num_leds - 1).
 * @param h Hue, in the range 0-359 degrees.
 * @param s Saturation, in the range 0-255.
 * @param v Value (brightness of the individual pixel), in the range 0-255.
 *          This value will be scaled by the global brightness set by `led_controller_set_brightness()`.
 * @return
 *      - ESP_OK: If the pixel color was set successfully in the buffer.
 *      - ESP_ERR_INVALID_STATE: If the LED controller is not initialized.
 *      - ESP_ERR_INVALID_ARG: If `index` is out of bounds.
 *      - Other ESP_ERR_*: Errors from the underlying `led_strip` component.
 */
esp_err_t led_controller_set_pixel_hsv(uint32_t index, uint16_t h, uint8_t s, uint8_t v);

/**
 * @brief Sets the overall brightness of the entire LED strip.
 *
 * This brightness value scales the 'V' component of all pixels.
 * The change is applied by the `led_strip` component, typically on the next refresh.
 *
 * @param brightness The global brightness level, from 0 (off) to 255 (full intensity).
 * @return
 *      - ESP_OK: If the brightness was set successfully.
 *      - ESP_ERR_INVALID_STATE: If the LED controller is not initialized.
 *      - Other ESP_ERR_*: Errors from the underlying `led_strip` component.
 */
esp_err_t led_controller_set_brightness(uint8_t brightness);

/**
 * @brief Turns the LED strip power effectively on or off.
 *
 * If `on` is false, this function will typically clear the LED strip (turn all LEDs off)
 * and refresh it. If `on` is true, it allows subsequent pixel operations to be visible
 * after a refresh. It doesn't necessarily restore a previous state but enables rendering.
 *
 * @param on Boolean value: true to enable LED rendering, false to disable and clear.
 * @return
 *      - ESP_OK: If the power state was handled successfully.
 *      - ESP_ERR_INVALID_STATE: If the LED controller is not initialized.
 */
esp_err_t led_controller_set_power(bool on);

/**
 * @brief Clears all LEDs in the buffer (sets them to off/black).
 *
 * `led_controller_refresh()` must be called afterwards to make the change visible
 * on the physical LED strip.
 *
 * @return
 *      - ESP_OK: If the LED buffer was cleared successfully.
 *      - ESP_ERR_INVALID_STATE: If the LED controller is not initialized.
 *      - Other ESP_ERR_*: Errors from the underlying `led_strip` component.
 */
esp_err_t led_controller_clear(void);

/**
 * @brief Transmits the current state of the LED buffer to the physical LED strip.
 *
 * This function must be called after any pixel manipulations (e.g., `led_controller_set_pixel_hsv`,
 * `led_controller_clear`) or brightness changes for those changes to become visible.
 *
 * @return
 *      - ESP_OK: If the LED strip was refreshed successfully.
 *      - ESP_ERR_INVALID_STATE: If the LED controller is not initialized.
 *      - Other ESP_ERR_*: Errors from the underlying `led_strip` component (e.g., SPI communication errors).
 */
esp_err_t led_controller_refresh(void);

/**
 * @brief Deinitializes the LED controller and releases associated resources.
 *
 * This includes deleting the LED strip handle. It does not deinitialize the SPI bus itself,
 * as the bus might be shared with other peripherals.
 *
 * @return
 *      - ESP_OK: If deinitialization was successful.
 *      - Other ESP_ERR_*: Errors from the underlying `led_strip` component during deletion.
 */
esp_err_t led_controller_deinit(void);

#ifdef __cplusplus
}
#endif

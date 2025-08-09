#include "led_controller.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/rmt.h"
#include "esp_log.h"
#include "fsm.h" // To get FSM state
#include "project_config.h"

static const char *TAG = "led_controller";

// RMT configuration for WS2812
#define RMT_TX_CHANNEL RMT_CHANNEL_0
#define BITS_PER_LED_CMD 24
#define LED_BUFFER_ITEMS ((LED_STRIP_LED_COUNT * BITS_PER_LED_CMD))

// Timings for WS2812
#define T0H 14 // 0 code, high level time (350ns)
#define T0L 36 // 0 code, low level time (900ns)
#define T1H 36 // 1 code, high level time (900ns)
#define T1L 14 // 1 code, low level time (350ns)

// Static variables
static led_controller_config_t controller_config;
static TaskHandle_t renderer_task_handle = NULL;
static SemaphoreHandle_t update_semaphore = NULL;
static rmt_item32_t *led_buffer = NULL;
static rmt_item32_t ws2812_bit0, ws2812_bit1;

// Forward declarations
static void led_renderer_task(void *pvParameters);
static void setup_rmt_data_buffer(const uint8_t *rgb_data);


esp_err_t led_controller_init(const led_controller_config_t *config) {
    ESP_LOGI(TAG, "Initializing LED controller");
    configASSERT(config != NULL);
    controller_config = *config;

    // Configure RMT
    rmt_config_t rmt_cfg = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_TX_CHANNEL,
        .gpio_num = controller_config.gpio_pin,
        .mem_block_num = 1,
        .clk_div = 2, // 80MHz / 2 = 40MHz clock -> 25ns period
        .tx_config.carrier_en = false,
        .tx_config.idle_output_en = true,
        .tx_config.idle_level = RMT_IDLE_LEVEL_LOW,
    };
    ESP_ERROR_CHECK(rmt_config(&rmt_cfg));
    ESP_ERROR_CHECK(rmt_driver_install(rmt_cfg.channel, 0, 0));

    // Set up timing for WS2812 bits
    uint32_t counter_clk_hz;
    ESP_ERROR_CHECK(rmt_get_counter_clock(RMT_TX_CHANNEL, &counter_clk_hz));
    float ratio = (float)counter_clk_hz / 1e9; // Clock ticks per nanosecond
    ws2812_bit0.duration0 = (uint32_t)(T0H * ratio);
    ws2812_bit0.level0 = 1;
    ws2812_bit0.duration1 = (uint32_t)(T0L * ratio);
    ws2812_bit0.level1 = 0;
    ws2812_bit1.duration0 = (uint32_t)(T1H * ratio);
    ws2812_bit1.level0 = 1;
    ws2812_bit1.duration1 = (uint32_t)(T1L * ratio);
    ws2812_bit1.level1 = 0;

    ESP_ERROR_CHECK(rmt_translator_init(RMT_TX_CHANNEL, NULL)); // Using custom translator

    // Allocate buffer
    led_buffer = (rmt_item32_t *)malloc(sizeof(rmt_item32_t) * LED_BUFFER_ITEMS);
    if (!led_buffer) {
        ESP_LOGE(TAG, "Failed to allocate LED buffer");
        return ESP_ERR_NO_MEM;
    }

    update_semaphore = xSemaphoreCreateBinary();
    if (update_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create update semaphore");
        free(led_buffer);
        return ESP_ERR_NO_MEM;
    }

    // Create renderer task
    BaseType_t task_created = xTaskCreate(led_renderer_task, "led_renderer_task",
                                          controller_config.task_stack_size, NULL,
                                          controller_config.task_priority, &renderer_task_handle);

    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create renderer task");
        vSemaphoreDelete(update_semaphore);
        free(led_buffer);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LED controller initialized successfully");
    return ESP_OK;
}

esp_err_t led_controller_deinit(void) {
    if (renderer_task_handle) {
        vTaskDelete(renderer_task_handle);
        renderer_task_handle = NULL;
    }
    if (update_semaphore) {
        vSemaphoreDelete(update_semaphore);
        update_semaphore = NULL;
    }
    if (led_buffer) {
        free(led_buffer);
        led_buffer = NULL;
    }
    ESP_ERROR_CHECK(rmt_driver_uninstall(RMT_TX_CHANNEL));
    ESP_LOGI(TAG, "LED controller de-initialized");
    return ESP_OK;
}

void led_controller_update_request(void) {
    if (update_semaphore != NULL) {
        xSemaphoreGive(update_semaphore);
    }
}

// Renders a solid color, applying brightness.
static void render_effect_solid(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness) {
    uint8_t pixels[LED_STRIP_LED_COUNT * 3];

    // Apply brightness
    uint8_t final_r = (uint8_t)(((uint16_t)r * brightness) / 255);
    uint8_t final_g = (uint8_t)(((uint16_t)g * brightness) / 255);
    uint8_t final_b = (uint8_t)(((uint16_t)b * brightness) / 255);

    for (uint32_t i = 0; i < LED_STRIP_LED_COUNT; i++) {
        pixels[i * 3 + 0] = final_g; // WS2812 is GRB order
        pixels[i * 3 + 1] = final_r;
        pixels[i * 3 + 2] = final_b;
    }
    setup_rmt_data_buffer(pixels);
    ESP_ERROR_CHECK(rmt_write_items(RMT_TX_CHANNEL, led_buffer, LED_BUFFER_ITEMS, true));
}

static void led_renderer_task(void *pvParameters) {
    ESP_LOGI(TAG, "Renderer task started");
    while (1) {
        // Wait for a request to update
        if (xSemaphoreTake(update_semaphore, portMAX_DELAY) == pdTRUE) {
            fsm_state_t current_state = fsm_get_current_state();
            uint8_t brightness = fsm_get_global_brightness();
            bool is_on = fsm_is_led_strip_on();

            if (!is_on) {
                render_effect_solid(0, 0, 0, 0); // Off
                continue;
            }

            switch (current_state) {
                case MODE_OFF:
                    render_effect_solid(0, 0, 0, 0); // Off
                    break;
                case MODE_DISPLAY:
                    // For now, just a solid color. Later this will be based on current_effect.
                    render_effect_solid(0, 0, 255, brightness); // Blue
                    break;
                case MODE_EFFECT_SELECT:
                    render_effect_solid(0, 255, 0, brightness); // Green
                    break;
                case MODE_EFFECT_SETUP:
                    render_effect_solid(255, 255, 0, brightness); // Yellow
                    break;
                case MODE_SYSTEM_SETUP:
                    render_effect_solid(128, 0, 128, brightness); // Purple
                    break;
                default:
                    render_effect_solid(255, 0, 0, brightness); // Red for unknown state
                    break;
            }
        }
    }
}

static void setup_rmt_data_buffer(const uint8_t *rgb_data) {
    uint32_t buffer_index = 0;
    for (uint32_t i = 0; i < controller_config.led_count; i++) {
        uint32_t r, g, b;
        // GRB order
        g = rgb_data[i * 3 + 0];
        r = rgb_data[i * 3 + 1];
        b = rgb_data[i * 3 + 2];
        uint32_t pixel_value = (g << 16) | (r << 8) | b;

        for (int j = BITS_PER_LED_CMD - 1; j >= 0; j--) {
            if ((pixel_value >> j) & 1) {
                led_buffer[buffer_index] = ws2812_bit1;
            } else {
                led_buffer[buffer_index] = ws2812_bit0;
            }
            buffer_index++;
        }
    }
}

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

// Project includes
#include "project_config.h"
#include "button.h"
#include "encoder.h"
#include "input_integrator.h"
#include "fsm.h"
#include "led_controller.h"

// For led_strip specific types used in config
#include "led_strip.h"
#include "driver/spi_common.h"

// --- Global Variables ---
static const char *TAG = "app_main";

static QueueHandle_t button_event_queue;
static QueueHandle_t encoder_event_queue;
static QueueHandle_t espnow_event_queue;
static QueueHandle_t integrated_event_queue;

static queue_manager_t queue_manager;

// --- Task Definitions ---
#define TASK_STACK_SIZE_INTEGRATOR 2048
#define TASK_STACK_SIZE_APP_LOGIC  4096


/**
 * @brief Main application logic task.
 *
 * This task runs a loop that reads the state from the FSM and updates the
 * LED controller accordingly. It creates a simple visual effect.
 */
static void app_logic_task(void *pvParameters) {
    ESP_LOGI(TAG, "Application logic task started.");
    uint16_t hue = 0;

    while (1) {
        // Get current state from the FSM
        bool is_on = fsm_is_led_strip_on();
        fsm_state_t state = fsm_get_current_state();
        uint8_t brightness = fsm_get_global_brightness();

        // Update LED controller based on FSM state
        led_controller_set_power(is_on);
        led_controller_set_brightness(brightness);

        if (is_on) {
            // Create a simple rainbow effect for demonstration
            for (int i = 0; i < LED_CONTROLLER_NUM_LEDS; i++) {
                // Calculate hue for each pixel, creating a rainbow pattern
                uint16_t pixel_hue = (hue + (i * 360 / LED_CONTROLLER_NUM_LEDS)) % 360;
                led_controller_set_pixel_hsv(i, pixel_hue, 255, 255);
            }
            // Cycle the hue for the next frame
            hue = (hue + 5) % 360;
        } else {
            // If the strip is off, we don't need to set pixels,
            // as set_power(false) already clears it.
            // We can just ensure it's cleared once.
            led_controller_clear();
        }

        // Refresh the strip to show the changes
        esp_err_t ret = led_controller_refresh();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to refresh LED strip: %s", esp_err_to_name(ret));
        }

        // Delay to control the animation speed
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


void app_main(void) {
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Starting application...");

    // --- Create Queues ---
    button_event_queue = xQueueCreate(BUTTON_QUEUE_SIZE, sizeof(button_event_t));
    configASSERT(button_event_queue != NULL);
    encoder_event_queue = xQueueCreate(ENCODER_QUEUE_SIZE, sizeof(encoder_event_t));
    configASSERT(encoder_event_queue != NULL);
    espnow_event_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(espnow_event_t));
    configASSERT(espnow_event_queue != NULL);
    UBaseType_t integrated_queue_len = BUTTON_QUEUE_SIZE + ENCODER_QUEUE_SIZE + ESPNOW_QUEUE_SIZE;
    integrated_event_queue = xQueueCreate(integrated_queue_len, sizeof(integrated_event_t));
    configASSERT(integrated_event_queue != NULL);

    // --- Initialize Input Components ---
    button_config_t btn_cfg = {
        .pin = BUTTON1_PIN,
        .active_low = true,
        .debounce_press_ms = DEBOUNCE_PRESS_MS,
        .debounce_release_ms = DEBOUNCE_RELEASE_MS,
        .double_click_ms = DOUBLE_CLICK_MS,
        .long_click_ms = LONG_CLICK_MS,
        .very_long_click_ms = VERY_LONG_CLICK_MS
    };
    button_t *button_handle = button_create(&btn_cfg, button_event_queue);
    configASSERT(button_handle != NULL);
    ESP_LOGI(TAG, "Button initialized on pin %d", BUTTON1_PIN);

    encoder_config_t enc_cfg = {
        .pin_a = ENCODER_PIN_A,
        .pin_b = ENCODER_PIN_B,
        .half_step_mode = false,
        .acceleration_enabled = true,
        .accel_gap_ms = ENC_ACCEL_GAP,
        .accel_max_multiplier = MAX_ACCEL_MULTIPLIER
    };
    encoder_handle_t encoder_handle = encoder_create(&enc_cfg, encoder_event_queue);
    configASSERT(encoder_handle != NULL);
    ESP_LOGI(TAG, "Encoder initialized on pins A:%d, B:%d", ENCODER_PIN_A, ENCODER_PIN_B);

    // --- Initialize Core Logic Components ---
    queue_manager = init_queue_manager(button_event_queue, encoder_event_queue, espnow_event_queue, integrated_event_queue);
    configASSERT(queue_manager.queue_set != NULL);
    ESP_LOGI(TAG, "Input integrator initialized.");

    configASSERT(fsm_init(integrated_event_queue, NULL) == ESP_OK);
    ESP_LOGI(TAG, "FSM initialized.");

    // --- Initialize LED Controller ---
    led_controller_config_t led_cfg = {
        .spi_host = LED_CONTROLLER_SPI_HOST,
        .clk_speed_hz = LED_CONTROLLER_CLK_SPEED_HZ,
        .num_leds = LED_CONTROLLER_NUM_LEDS,
        .spi_mosi_gpio = LED_CONTROLLER_MOSI_GPIO,
        .spi_sclk_gpio = LED_CONTROLLER_SCLK_GPIO,
        .pixel_format = LED_CONTROLLER_COLOR_FORMAT,
        .model = LED_CONTROLLER_MODEL,
        .spi_clk_src = LED_CONTROLLER_SPI_CLK_SRC,
    };
    configASSERT(led_controller_init(&led_cfg) == ESP_OK);
    ESP_LOGI(TAG, "LED controller initialized.");

    // --- Create Tasks ---
    BaseType_t task_created;
    task_created = xTaskCreate(integrator_task, "integrator_task", TASK_STACK_SIZE_INTEGRATOR, &queue_manager, 5, NULL);
    configASSERT(task_created == pdPASS);

    task_created = xTaskCreate(app_logic_task, "app_logic_task", TASK_STACK_SIZE_APP_LOGIC, NULL, 4, NULL);
    configASSERT(task_created == pdPASS);

    ESP_LOGI(TAG, "System setup complete. All tasks running.");
}

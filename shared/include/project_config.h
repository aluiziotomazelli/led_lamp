#pragma once

#include "esp_log.h"
#include "esp_debug_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================================================
// GPIO Pin Configuration
// ==================================================
#define BUTTON1_PIN          23      			// Physical button GPIO pin
#define ENCODER_PIN_A        17      			// Rotary encoder channel A
#define ENCODER_PIN_B        16      			// Rotary encoder channel B
#define TOUCH_PAD1_PIN       TOUCH_PAD_NUM0  	// GPIO4 for touch button
#define SWITCH_PIN_1         32                 // GPIO for the mode switch

// ==================================================
// Queue Configuration
// ==================================================
#define BUTTON_QUEUE_SIZE     5      // Button event queue size
#define ENCODER_QUEUE_SIZE   10      // Encoder event queue size
#define TOUCH_QUEUE_SIZE      5      // Touch event queue size
#define SWITCH_QUEUE_SIZE     5      // Switch event queue size
#define ESPNOW_QUEUE_SIZE    10      // ESPNOW message queue size
#define LED_CMD_QUEUE_SIZE   10

// ==================================================
// Task Configuration
// ==================================================
// Stack sizes (in bytes)
#define BUTTON_TASK_STACK_SIZE   2044
#define ENCODER_TASK_STACK_SIZE  3072
#define TOUCH_TASK_STACK_SIZE    2048
#define ESPNOW_TASK_STACK_SIZE   4096
#define FSM_STACK_SIZE           4096   // Finite State Machine stack
#define INTEGRATOR_TASK_STACK_SIZE 4096
#define LED_CTRL_STACK_SIZE			4069
#define LED_RENDER_STACK_SIZE			4069
#define LED_DRIVER_TASK_STACK_SIZE  4096     // Stack size for the LED driver task
#define SWITCH_TASK_STACK_SIZE   2048

// Task priorities
#define BUTTON_TASK_PRIORITY     10
#define ENCODER_TASK_PRIORITY    10
#define TOUCH_TASK_PRIORITY      10
#define SWITCH_TASK_PRIORITY     10
#define ESPNOW_TASK_PRIORITY     10
#define FSM_TASK_PRIORITY         5
#define INTEGRATOR_TASK_PRIORITY  5
#define LED_CTRL_TASK_PRIORITY    5
#define LED_RENDER_TASK_PRIORITY    5
#define LED_DRIVER_TASK_PRIORITY    5        // Priority for the LED driver task

// ==================================================
// Touch Button Configuration
// ==================================================
#define TOUCH_THRESHOLD_PERCENT          60    // Activation threshold (% of baseline)
#define TOUCH_DEBOUNCE_PRESS_MS          20    // Press debounce time
#define TOUCH_DEBOUNCE_RELEASE_MS        20    // Release debounce time
#define TOUCH_HOLD_TIME_MS              1000   // Time to trigger hold event (1s)
#define TOUCH_HOLD_REPEAT_TIME_MS       200    // Interval between hold repeats
#define TOUCH_SAMPLE_INTERVAL           100    // Sampling interval (ms)
#define TOUCH_RECALIBRATION_INTERVAL_MIN 1     // Auto-recalibration interval (minutes)

// ==================================================
// Physical Button Configuration
// ==================================================
#define DEBOUNCE_PRESS_MS        50     // Press debounce time
#define DEBOUNCE_RELEASE_MS      30     // Release debounce time
#define DOUBLE_CLICK_MS         180     // Max interval between clicks for double-click
#define LONG_CLICK_MS          1000     // Time for long press (1s)
#define VERY_LONG_CLICK_MS     3000     // Time for very long press (3s)

// ==================================================
// Rotary Encoder Configuration
// ==================================================
#define ENC_ACCEL_GAP           50     // Time gap for acceleration (ms)
#define MAX_ACCEL_MULTIPLIER     5     // Maximum acceleration multiplier

// ==================================================
// Finite State Machine Configuration
// ==================================================
#define FSM_TIMEOUT_MS         100     // Default state timeout
#define FSM_MODE_TIMEOUT_MS   30000    // Mode change timeout (30s)

// ==================================================
// LED Controller Configuration
// ==================================================
#define NUM_LEDS 10 // Number of LEDs in the strip
#define LED_RENDER_INTERVAL_MS 10
#define MIN_BRIGHTNESS 20
#define FADE_DURATION_MS 5000
// Default values for LED offsets, used for resetting
#define DEFAULT_LED_OFFSET_BEGIN 2
#define DEFAULT_LED_OFFSET_END   2

// Global variables for LED offsets, allowing runtime modification.
// These are defined in led_controller.c
extern uint16_t g_led_offset_begin;
extern uint16_t g_led_offset_end;
// ==================================================
// LED Driver Configuration
// ==================================================
#define LED_STRIP_GPIO              13        // GPIO for the LED strip data line
#define LED_STRIP_SPI_HOST          SPI2_HOST // SPI host for the LED strip

// ==================================================
// ESP-NOW Configuration
// ==================================================

// Master MAC: (08:3a:f2:ac:50:dc)
// 0x08, 0x3A, 0xF2, 0xAC, 0x50, 0xDC

// Slave MAC: (84:cc:a8:7a:66:e0)
// 0x84, 0xCC, 0xA8, 0x7A, 0x66, 0xE0


#define ESP_NOW_ENABLED  1 // 1 to enable, 0 to disable
#define IS_MASTER        1 // 1 for master, 0 for slave
#define IS_SLAVE         0 // 1 for slave, 0 for master

// Set to 0 to disable all feedback animations (blinks) on the slave device.
#define SLAVE_ENABLE_FEEDBACK 0

// List of slave MAC addresses for the master to send to.
// The master will not be a slave, and a slave will not be a master.
// Replace with the actual MAC addresses of your slave devices.
static uint8_t slave_mac_addresses[][6] = {
    {0x84, 0xCC, 0xA8, 0x7A, 0x66, 0xE0}
// Para mais slaves descomentar as linhas abaixo e troca pelo MAC real
//    ,
//    {0x7A, 0x8B, 0x9C, 0xAD, 0xBE, 0xCF}
};
static const int num_slaves = sizeof(slave_mac_addresses) / sizeof(slave_mac_addresses[0]);


// ==================================================
// System Assertion Configuration
// ==================================================
#ifndef configASSERT
#define configASSERT(x) do { \
    if (!(x)) { \
        ESP_LOGE("ASSERT", "Failed at %s:%d (%s)", __FILE__, __LINE__, #x); \
        if (CONFIG_ESP_SYSTEM_PANIC_PRINT_BACKTRACE) { \
            esp_backtrace_dump(); \
        } \
        while(1) { vTaskDelay(pdMS_TO_TICKS(1000)); } \
    } \
} while (0)
#endif

#ifdef __cplusplus
}
#endif
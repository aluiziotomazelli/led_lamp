/**
 * @file project_config.h
 * @brief Project-wide configuration and hardware definitions
 * 
 * @details This header contains all system-wide configuration parameters,
 *          pin assignments, task priorities, queue sizes, and hardware-specific
 *          settings for the LED controller project.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

// System includes
#include "esp_log.h"
#include "esp_debug_helpers.h"


// ==================================================
// GPIO Pin Configuration
// ==================================================

/** @brief Physical button GPIO pin number */
#define BUTTON1_PIN          23

/** @brief Rotary encoder channel A GPIO pin number */
#define ENCODER_PIN_A        17

/** @brief Rotary encoder channel B GPIO pin number */
#define ENCODER_PIN_B        16

/** @brief Touch button pad number (GPIO4) */
#define TOUCH_PAD1_PIN       TOUCH_PAD_NUM0

/** @brief Mode switch GPIO pin number */
#define SWITCH_PIN_1         32

// ==================================================
// Queue Configuration
// ==================================================

/** @brief Button event queue size (number of events) */
#define BUTTON_QUEUE_SIZE     5

/** @brief Encoder event queue size (number of events) */
#define ENCODER_QUEUE_SIZE   10

/** @brief Touch event queue size (number of events) */
#define TOUCH_QUEUE_SIZE      5

/** @brief Switch event queue size (number of events) */
#define SWITCH_QUEUE_SIZE     5

/** @brief ESPNOW message queue size (number of messages) */
#define ESPNOW_QUEUE_SIZE    10

/** @brief LED command queue size (number of commands) */
#define LED_CMD_QUEUE_SIZE   10

// ==================================================
// Task Configuration
// ==================================================

// Stack sizes (in bytes)

/** @brief Button task stack size in bytes */
#define BUTTON_TASK_STACK_SIZE   	2044

/** @brief Encoder task stack size in bytes */
#define ENCODER_TASK_STACK_SIZE  	3072

/** @brief Touch task stack size in bytes */
#define TOUCH_TASK_STACK_SIZE    	2048

/** @brief ESPNOW task stack size in bytes */
#define ESPNOW_TASK_STACK_SIZE   	4096

/** @brief Finite State Machine task stack size in bytes */
#define FSM_STACK_SIZE           	4096

/** @brief Input integrator task stack size in bytes */
#define INTEGRATOR_TASK_STACK_SIZE 	4096

/** @brief LED controller task stack size in bytes */
#define LED_CTRL_STACK_SIZE			4069

/** @brief LED render task stack size in bytes */
#define LED_RENDER_STACK_SIZE		4069

/** @brief LED driver task stack size in bytes */
#define LED_DRIVER_TASK_STACK_SIZE  4096

/** @brief Switch task stack size in bytes */
#define SWITCH_TASK_STACK_SIZE   	2048

// Task priorities (higher number = higher priority)

/** @brief LED driver task priority (hard real-time) */
#define LED_DRIVER_TASK_PRIORITY    15

/** @brief LED render task priority (soft real-time) */
#define LED_RENDER_TASK_PRIORITY    14

/** @brief Button task priority - Responsive input handling */
#define BUTTON_TASK_PRIORITY        10

/** @brief Encoder task priority - Responsive input handling  */
#define ENCODER_TASK_PRIORITY       10

/** @brief Touch task priority - Responsive input handling  */
#define TOUCH_TASK_PRIORITY         10

/** @brief Switch task priority - Responsive input handling  */
#define SWITCH_TASK_PRIORITY        10

/** @brief Input integrator task priority - Bridges inputs to the FSM */
#define INTEGRATOR_TASK_PRIORITY    8

/** @brief Finite State Machine task priority - Main application logic */
#define FSM_TASK_PRIORITY           7

/** @brief LED controller task priority - Handles commands from the FSM */
#define LED_CTRL_TASK_PRIORITY      7

/** @brief ESPNOW task priority (non-critical) */
#define ESPNOW_TASK_PRIORITY        6

// ==================================================
// Touch Button Configuration
// ==================================================

/** @brief Touch activation threshold (% of baseline value) */
#define TOUCH_THRESHOLD_PERCENT          60

/** @brief Press debounce time in milliseconds */
#define TOUCH_DEBOUNCE_PRESS_MS          20

/** @brief Release debounce time in milliseconds */
#define TOUCH_DEBOUNCE_RELEASE_MS        20

/** @brief Time to trigger hold event in milliseconds */
#define TOUCH_HOLD_TIME_MS              1000

/** @brief Interval between hold repeat events in milliseconds */
#define TOUCH_HOLD_REPEAT_TIME_MS       200

/** @brief Touch sampling interval in milliseconds */
#define TOUCH_SAMPLE_INTERVAL           100

/** @brief Auto-recalibration interval in minutes */
#define TOUCH_RECALIBRATION_INTERVAL_MIN 1

// ==================================================
// Physical Button Configuration
// ==================================================

/** @brief Physical button press debounce time in milliseconds */
#define DEBOUNCE_PRESS_MS        50

/** @brief Physical button release debounce time in milliseconds */
#define DEBOUNCE_RELEASE_MS      30

/** @brief Maximum interval between clicks for double-click detection in milliseconds */
#define DOUBLE_CLICK_MS         180

/** @brief Long press duration in milliseconds */
#define LONG_CLICK_MS          1000

/** @brief Very long press duration in milliseconds */
#define VERY_LONG_CLICK_MS     3000

// ==================================================
// Rotary Encoder Configuration
// ==================================================

/** @brief Time gap for acceleration activation in milliseconds */
#define ENC_ACCEL_GAP           100

/** @brief Maximum acceleration multiplier value */
#define MAX_ACCEL_MULTIPLIER     10

// ==================================================
// Finite State Machine Configuration
// ==================================================

/** @brief Default FSM state timeout in milliseconds */
#define FSM_TIMEOUT_MS         100

/** @brief Mode change timeout duration in milliseconds */
#define FSM_MODE_TIMEOUT_MS   30000

// ==================================================
// LED Controller Configuration
// ==================================================

/** @brief Number of LEDs in the strip */
#define NUM_LEDS 48

/** @brief LED render interval in milliseconds */
#define LED_RENDER_INTERVAL_MS 10

// Default values for configurable parameters

/** @brief Default minimum brightness value (0-255) */
#define DEFAULT_MIN_BRIGHTNESS 20

/** @brief Default LED strip beginning offset */
#define DEFAULT_LED_OFFSET_BEGIN 0

/** @brief Default LED strip ending offset */
#define DEFAULT_LED_OFFSET_END   0

// Global configurable parameters (defined in led_controller.c)

/** @brief Global minimum brightness setting */
extern uint8_t g_min_brightness;

/** @brief Global LED strip beginning offset */
extern uint16_t g_led_offset_begin;

/** @brief Global LED strip ending offset */
extern uint16_t g_led_offset_end;

// ==================================================
// LED Driver Configuration
// ==================================================

/** @brief GPIO pin for LED strip data line */
#define LED_STRIP_GPIO              13

/** @brief SPI host for LED strip communication */
#define LED_STRIP_SPI_HOST          SPI2_HOST

// ==================================================
// ESP-NOW Configuration
// ==================================================

// MAC addresses preserved as requested - used during compilation
// Master MAC: (08:3a:f2:ac:50:dc)
// 0x08, 0x3A, 0xF2, 0xAC, 0x50, 0xDC

// Slave MAC: (84:cc:a8:7a:66:e0)
// 0x84, 0xCC, 0xA8, 0x7A, 0x66, 0xE0

/** @brief Enable/disable ESP-NOW functionality (1 = enable, 0 = disable) */
#define ESP_NOW_ENABLED  1

/** @brief Device role: master (1 = master, 0 = not master) */
#define IS_MASTER        1

/** @brief Device role: slave (1 = slave, 0 = not slave) */
#define IS_SLAVE         0

/** @brief Enable/disable feedback animations on slave device (1 = enable, 0 = disable) */
#define SLAVE_ENABLE_FEEDBACK 0

/**
 * @brief Array of slave MAC addresses for master device
 * 
 * @note The master will not be a slave, and a slave will not be a master.
 *       Replace with actual MAC addresses of slave devices.
 */
static uint8_t slave_mac_addresses[][6] = {
    {0x84, 0xCC, 0xA8, 0x7A, 0x66, 0xE0}
// Para mais slaves descomentar as linhas abaixo e troca pelo MAC real
//    ,
//    {0x7A, 0x8B, 0x9C, 0xAD, 0xBE, 0xCF}
};

/** @brief Number of slave devices in the array */
static const int num_slaves = sizeof(slave_mac_addresses) / sizeof(slave_mac_addresses[0]);

// ==================================================
// System Assertion Configuration
// ==================================================

#ifndef configASSERT
/**
 * @brief Custom assertion macro with backtrace dump
 * 
 * @details Provides detailed assertion failure information including
 *          file, line number, and backtrace when enabled.
 */
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
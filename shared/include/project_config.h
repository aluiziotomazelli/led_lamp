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
#define BUTTON1_PIN          23 			// Physical button GPIO pin number
#define ENCODER_PIN_A        17 			// Rotary encoder channel A GPIO pin number
#define ENCODER_PIN_B        16 			// Rotary encoder channel B GPIO pin number
#define TOUCH_PAD1_PIN       TOUCH_PAD_NUM0 // Touch button pad number (GPIO4)
#define SWITCH_PIN_1         32 			// Mode switch GPIO pin number

// ==================================================
// Queue Configuration
// ==================================================
#define BUTTON_QUEUE_SIZE     5 	// Button event queue size (number of events)
#define ENCODER_QUEUE_SIZE   10		// Encoder event queue size (number of events)
#define TOUCH_QUEUE_SIZE      5  	// Touch event queue size (number of events)
#define SWITCH_QUEUE_SIZE     5  	// Switch event queue size (number of events)
#define ESPNOW_QUEUE_SIZE    10 	// ESPNOW message queue size (number of messages)
#define LED_CMD_QUEUE_SIZE   10 	// LED command queue size (number of commands)
#define LED_STRIP_QUEUE_SIZE  3 	// LED strip data queue size (number of frames)

// ==================================================
// Task Configuration
// ==================================================

// Stack sizes (in bytes)
#define BUTTON_TASK_STACK_SIZE   	2044 // Button task stack size in bytes
#define ENCODER_TASK_STACK_SIZE  	3072 // Encoder task stack size in bytes
#define TOUCH_TASK_STACK_SIZE    	2048 // Touch task stack size in bytes
#define ESPNOW_TASK_STACK_SIZE   	4096 // ESPNOW task stack size in bytes
#define FSM_STACK_SIZE           	4096 // Finite State Machine task stack size in bytes
#define INTEGRATOR_TASK_STACK_SIZE 	4096 // Input integrator task stack size in bytes
#define LED_CTRL_STACK_SIZE			4069 // LED controller task stack size in bytes
#define LED_RENDER_STACK_SIZE		4069 // LED render task stack size in bytes
#define LED_DRIVER_TASK_STACK_SIZE  4096 // LED driver task stack size in bytes
#define SWITCH_TASK_STACK_SIZE   	2048 // Switch task stack size in bytes

// Task priorities (higher number = higher priority)
#define LED_DRIVER_TASK_PRIORITY    15 // LED driver task priority (hard real-time)
#define LED_RENDER_TASK_PRIORITY    14 // LED render task priority (soft real-time)
#define BUTTON_TASK_PRIORITY        10 // Button task priority - Responsive input handling
#define ENCODER_TASK_PRIORITY       10 // Encoder task priority - Responsive input handling
#define TOUCH_TASK_PRIORITY         10 // Touch task priority - Responsive input handling
#define SWITCH_TASK_PRIORITY        10 // Switch task priority - Responsive input handling
#define INTEGRATOR_TASK_PRIORITY     8 // Input integrator task priority - Bridges inputs to the FSM
#define FSM_TASK_PRIORITY            7 // Finite State Machine task priority - Main application logic
#define LED_CTRL_TASK_PRIORITY       7 // LED controller task priority - Handles commands from the FSM
#define ESPNOW_TASK_PRIORITY         6 // ESPNOW task priority (non-critical)

// ==================================================
// Touch Button Configuration
// ==================================================
#define TOUCH_THRESHOLD_PERCENT           60   	// Touch activation threshold (% of baseline value)
#define TOUCH_DEBOUNCE_PRESS_MS           20   	// Press debounce time in milliseconds
#define TOUCH_DEBOUNCE_RELEASE_MS         20   	// Release debounce time in milliseconds
#define TOUCH_HOLD_TIME_MS              1000  	// Time to trigger hold event in milliseconds
#define TOUCH_HOLD_REPEAT_TIME_MS        200   	// Interval between hold repeat events in milliseconds
#define TOUCH_SAMPLE_INTERVAL            100   	// Touch sampling interval in milliseconds
#define TOUCH_RECALIBRATION_INTERVAL_MIN   1	// Auto-recalibration interval in minutes

// ==================================================
// Physical Button Configuration
// ==================================================
#define DEBOUNCE_PRESS_MS        50   // Physical button press debounce time in milliseconds
#define DEBOUNCE_RELEASE_MS      30   // Physical button release debounce time in milliseconds
#define DOUBLE_CLICK_MS         180   // Maximum interval between clicks for double-click detection in milliseconds
#define LONG_CLICK_MS          1000   // Long press duration in milliseconds
#define VERY_LONG_CLICK_MS     3000   // Very long press duration in milliseconds

// ==================================================
// Rotary Encoder Configuration
// ==================================================
#define ENC_ACCEL_GAP           100 // Time gap for acceleration activation in milliseconds
#define MAX_ACCEL_MULTIPLIER     10 // Maximum acceleration multiplier value

// ==================================================
// Finite State Machine Configuration
// ==================================================
#define FSM_TIMEOUT_MS          100 	// Default FSM state timeout in milliseconds
#define FSM_MODE_TIMEOUT_MS   30000 	// Mode change timeout duration in milliseconds

// ==================================================
// LED Controller Configuration
// ==================================================
#define NUM_LEDS 				48 	// Number of LEDs in the strip
#define LED_RENDER_INTERVAL_MS 	10 	// LED render interval in milliseconds

// Default values for configurable parameters
#define DEFAULT_MIN_BRIGHTNESS 	20 // Default minimum brightness value (0-255)
#define DEFAULT_LED_OFFSET_BEGIN 0 // Default LED strip beginning offset
#define DEFAULT_LED_OFFSET_END   0 // Default LED strip ending offset

// ==================================================
// LED Driver Configuration
// ==================================================
#define LED_STRIP_GPIO              13 // GPIO pin for LED strip data line
#define LED_STRIP_SPI_HOST          SPI2_HOST // SPI host for LED strip communication

// ==================================================
// ESP-NOW Configuration
// ==================================================
// MAC addresses preserved as requested - used during compilation
// Master MAC: (08:3a:f2:ac:50:dc)
// 0x08, 0x3A, 0xF2, 0xAC, 0x50, 0xDC

// Slave MAC: (84:cc:a8:7a:66:e0)
// 0x84, 0xCC, 0xA8, 0x7A, 0x66, 0xE0

#define ESP_NOW_ENABLED  1 // Enable/disable ESP-NOW functionality (1 = enable, 0 = disable)
#define IS_MASTER        0 // Device role: master (1 = master, 0 = not master)
#define IS_SLAVE         1 // Device role: slave (1 = slave, 0 = not slave)
#define SLAVE_ENABLE_FEEDBACK 0 // Enable/disable feedback animations on slave device (1 = enable, 0 = disable)

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

static const int num_slaves = sizeof(slave_mac_addresses) / sizeof(slave_mac_addresses[0]); // Number of slave devices in the array

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
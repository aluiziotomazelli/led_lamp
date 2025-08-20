#pragma once

/**
 * @brief Initializes the power manager and configures light sleep wakeup sources.
 *
 * This function must be called once during application startup. It sets up
 * the necessary wakeup triggers (e.g., GPIO, Wi-Fi) based on the device's
 * role (Master or Slave).
 */
void power_manager_init(void);

/**
 * @brief Enters light sleep mode.
 *
 * This function puts the ESP32 into light sleep. The device will wake up
 * when one of the configured wakeup sources is triggered.
 * Execution will resume from the point where this function was called.
 */
void power_manager_enter_sleep(void);
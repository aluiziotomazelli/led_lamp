#pragma once

/**
 * @brief Initializes the power manager and configures light sleep wakeup sources.
 *
 * This function must be called once during application startup.
 *
 * @param btn_handle A handle to the button component instance, used to reset
 *                   the button's state before sleeping.
 */
void power_manager_init(button_t *btn_handle);

/**
 * @brief Enters light sleep mode.
 *
 * This function puts the ESP32 into light sleep. The device will wake up
 * when one of the configured wakeup sources is triggered.
 * Execution will resume from the point where this function was called.
 */
void power_manager_enter_sleep(void);
/**
 * @file espnow_controller.h
 * @brief ESP-NOW communication controller for master/slave devices
 * @author Your Name
 * @version 1.0
 */

#pragma once

// Project includes
#include "fsm.h"

/**
 * @brief The data payload for ESP-NOW messages
 *
 * This structure encapsulates the command that needs to be synced
 * between the master and slave devices.
 */
typedef struct {
    led_command_t cmd;  ///< LED command to be synchronized
} espnow_message_t;

/**
 * @brief Initializes the ESP-NOW controller
 *
 * This function sets up Wi-Fi, initializes ESP-NOW, and registers the
 * necessary callbacks for sending and receiving data. It should be called
 * from app_main.
 *
 * @param[in] espnow_queue The queue to which received ESP-NOW events will be posted
 * 
 * @note This function must be called after WiFi is initialized
 * @warning ESP-NOW functionality depends on project configuration flags
 */
void espnow_controller_init(QueueHandle_t espnow_queue);

/**
 * @brief Sends an ESP-NOW message to all registered slave peers
 *
 * This function is called by the master device to broadcast a command.
 *
 * @param[in] msg The message to send
 * 
 * @note Only works when compiled as MASTER with ESP_NOW_ENABLED
 */
void espnow_controller_send(const espnow_message_t *msg);

/**
 * @brief Enables or disables the master's sending functionality
 *
 * @param[in] enabled True to enable sending, false to disable
 * 
 * @note Only effective when compiled as MASTER
 */
void espnow_controller_set_master_enabled(bool enabled);

/**
 * @brief Checks if the master's sending functionality is currently enabled
 *
 * @return True if sending is enabled, false otherwise
 * 
 * @note Always returns false when compiled as SLAVE
 */
bool espnow_controller_is_master_enabled(void);
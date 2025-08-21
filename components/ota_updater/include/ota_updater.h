/**
 * @file ota_updater.h
 * @brief OTA update via ESP32's own server
 * 
 * @details Based on implementation from:
 *          https://github.com/Jeija/esp32-softap-ota/tree/master
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

#include "esp_err.h"

/**
 * @brief Starts the SoftAP OTA update process.
 *
 * @details This function clears the OTA flag, initializes Wi-Fi in Access Point mode,
 *          and starts an HTTP server to allow firmware uploads from a client.
 *
 * @return esp_err_t ESP_OK if the OTA process is started successfully, otherwise an error code.
 */
esp_err_t ota_updater_start(void);
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

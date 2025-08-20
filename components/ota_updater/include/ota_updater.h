#pragma once

#include "esp_err.h"
#include "nvs_manager.h" // To pass ota_data_t

/**
 * @brief Starts the OTA update process.
 *
 * @details This function initializes Wi-Fi, connects to the specified network,
 *          and starts a task to perform the firmware update from a predefined URL.
 *          The device will restart upon successful update.
 *
 * @param[in] ota_data Pointer to the OTA configuration data containing Wi-Fi credentials.
 * @return esp_err_t ESP_OK if the OTA task is started successfully, otherwise an error code.
 */
esp_err_t ota_updater_start(const ota_data_t *ota_data);

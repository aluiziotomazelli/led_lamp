#pragma once

#include "nvs_data.h"
#include "esp_err.h"

// NVS Namespace and Keys for storing the data structures
#define NVS_NAMESPACE "led_config"
#define KEY_VOLATILE_DATA "volatile"
#define KEY_STATIC_DATA   "static"

/**
 * @brief Initializes the NVS manager.
 *
 * Currently, this function does not perform any specific operations, as NVS
 * flash initialization is handled in `main.c`. It's included for future
 * scalability and to maintain a consistent module structure.
 *
 * @return ESP_OK.
 */
esp_err_t nvs_manager_init(void);

/**
 * @brief Saves the volatile data structure to NVS.
 *
 * @param data A pointer to the `volatile_data_t` struct containing the data to be saved.
 * @return `ESP_OK` on successful save, or an `esp_err_t` error code on failure.
 */
esp_err_t nvs_manager_save_volatile_data(const volatile_data_t *data);

/**
 * @brief Loads the volatile data structure from NVS.
 *
 * If the data is not found in NVS (e.g., on the first boot), this function
 * will populate the `data` struct with sensible default values.
 *
 * @param data A pointer to the `volatile_data_t` struct to be filled with the loaded data.
 * @return `ESP_OK` on successful load, or an `esp_err_t` error code on failure.
 */
esp_err_t nvs_manager_load_volatile_data(volatile_data_t *data);

/**
 * @brief Saves the static data structure to NVS.
 *
 * @param data A pointer to the `static_data_t` struct containing the data to be saved.
 * @return `ESP_OK` on successful save, or an `esp_err_t` error code on failure.
 */
esp_err_t nvs_manager_save_static_data(const static_data_t *data);

/**
 * @brief Loads the static data structure from NVS.
 *
 * If the data is not found in NVS (e.g., on the first boot), this function
 * will populate the `data` struct with sensible default values from `project_config.h`
 * and `led_effects.c`.
 *
 * @param data A pointer to the `static_data_t` struct to be filled with the loaded data.
 * @return `ESP_OK` on successful load, or an `esp_err_t` error code on failure.
 */
esp_err_t nvs_manager_load_static_data(static_data_t *data);

/**
 * @file nvs_manager.h
 * @brief NVS Manager interface for LED configuration storage
 * 
 * @details This header provides the interface for saving and loading LED configuration
 *          data to/from non-volatile storage using ESP-IDF's NVS library.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

#pragma once

// Project specific headers
#include "nvs_data.h"

// ESP-IDF system services
#include "esp_err.h"

/**
 * @brief NVS Namespace and Keys for storing the data structures
 */
#define NVS_NAMESPACE "led_config"          ///< NVS namespace for LED configuration
#define KEY_VOLATILE_DATA "volatile"        ///< Key for volatile data storage
#define KEY_STATIC_DATA   "static"          ///< Key for static data storage

/**
 * @brief Initializes the NVS manager
 * 
 * @details Currently, this function does not perform any specific operations, as NVS
 *          flash initialization is handled in `main.c`. It's included for future
 *          scalability and to maintain a consistent module structure.
 * 
 * @return ESP_OK on success
 */
esp_err_t nvs_manager_init(void);

/**
 * @brief Saves the volatile data structure to NVS
 * 
 * @param[in] data Pointer to the volatile_data_t struct containing data to save
 * @return ESP_OK on successful save, or esp_err_t error code on failure
 */
esp_err_t nvs_manager_save_volatile_data(const volatile_data_t *data);

/**
 * @brief Loads the volatile data structure from NVS
 * 
 * @details If the data is not found in NVS (e.g., on first boot), this function
 *          will populate the data struct with sensible default values.
 * 
 * @param[out] data Pointer to the volatile_data_t struct to be filled
 * @return ESP_OK on successful load, or esp_err_t error code on failure
 */
esp_err_t nvs_manager_load_volatile_data(volatile_data_t *data);

/**
 * @brief Saves the static data structure to NVS
 * 
 * @param[in] data Pointer to the static_data_t struct containing data to save
 * @return ESP_OK on successful save, or esp_err_t error code on failure
 */
esp_err_t nvs_manager_save_static_data(const static_data_t *data);

/**
 * @brief Loads the static data structure from NVS
 * 
 * @details If the data is not found in NVS (e.g., on first boot), this function
 *          will populate the data struct with sensible default values from
 *          project_config.h and led_effects.c.
 * 
 * @param[out] data Pointer to the static_data_t struct to be filled
 * @return ESP_OK on successful load, or esp_err_t error code on failure
 */
esp_err_t nvs_manager_load_static_data(static_data_t *data);
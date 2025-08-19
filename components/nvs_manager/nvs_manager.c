/**
 * @file nvs_manager.c
 * @brief NVS Manager implementation for LED configuration storage
 * 
 * @details This file implements the non-volatile storage management for both
 *          volatile and static LED configuration data using ESP-IDF's NVS library.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <string.h>

// ESP-IDF system services
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

// Project specific headers
#include "nvs_manager.h"
#include "project_config.h"
#include "led_effects.h"

static const char *TAG = "NVS_MANAGER";

//------------------------------------------------------------------------------
// PRIVATE VARIABLES
//------------------------------------------------------------------------------

/// @brief External reference to effects array from led_effects.c
extern effect_t *effects[];

/// @brief External reference to effects count from led_effects.c
extern const uint8_t effects_count;

//------------------------------------------------------------------------------
// PRIVATE FUNCTION DECLARATIONS
//------------------------------------------------------------------------------

/**
 * @brief Loads default values for the volatile_data_t struct
 * 
 * @param[out] data Pointer to volatile_data_t to populate with defaults
 */
static void load_volatile_defaults(volatile_data_t *data);

/**
 * @brief Loads default values for the static_data_t struct
 * 
 * @param[out] data Pointer to static_data_t to populate with defaults
 */
static void load_static_defaults(static_data_t *data);

//------------------------------------------------------------------------------
// PRIVATE FUNCTION IMPLEMENTATIONS
//------------------------------------------------------------------------------

/**
 * @brief Load default values for volatile data
 */
static void load_volatile_defaults(volatile_data_t *data) {
    ESP_LOGI(TAG, "Loading default volatile data.");
    data->is_on = true;
    data->master_brightness = 255; // Default to max brightness
    data->effect_index = 0;        // Default to the first effect
}

/**
 * @brief Load default values for static data
 */
static void load_static_defaults(static_data_t *data) {
    ESP_LOGI(TAG, "Loading default static data.");
    data->min_brightness = DEFAULT_MIN_BRIGHTNESS;
    data->led_offset_begin = DEFAULT_LED_OFFSET_BEGIN;
    data->led_offset_end = DEFAULT_LED_OFFSET_END;

    // Load default parameters for each effect
    for (uint8_t i = 0; i < effects_count && i < NVS_NUM_EFFECTS; i++) {
        for (uint8_t j = 0; j < effects[i]->num_params && j < NVS_MAX_PARAMS_PER_EFFECT; j++) {
            data->effect_params[i][j] = effects[i]->params[j].default_value;
        }
    }
}

//------------------------------------------------------------------------------
// PUBLIC FUNCTION IMPLEMENTATIONS
//------------------------------------------------------------------------------

/**
 * @brief Initialize NVS manager
 */
esp_err_t nvs_manager_init(void) {
    // nvs_flash_init() is called in main.c, so nothing to do here for now
    ESP_LOGI(TAG, "NVS manager initialized.");
    return ESP_OK;
}

/**
 * @brief Save volatile data to NVS
 */
esp_err_t nvs_manager_save_volatile_data(const volatile_data_t *data) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, KEY_VOLATILE_DATA, data, sizeof(volatile_data_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing volatile data to NVS: %s", esp_err_to_name(err));
    } else {
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error committing volatile data to NVS: %s", esp_err_to_name(err));
        } else {
            ESP_LOGD(TAG, "Volatile data saved successfully.");
        }
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Load volatile data from NVS
 */
esp_err_t nvs_manager_load_volatile_data(volatile_data_t *data) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS namespace '%s' not found. Loading defaults.", NVS_NAMESPACE);
        load_volatile_defaults(data);
        return ESP_ERR_NVS_NOT_FOUND; // Return specific error to signal defaults loaded
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        load_volatile_defaults(data); // Load defaults on any other error for safety
        return err;
    }

    size_t required_size = sizeof(volatile_data_t);
    err = nvs_get_blob(nvs_handle, KEY_VOLATILE_DATA, data, &required_size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Volatile data not found in NVS. Loading defaults.");
        load_volatile_defaults(data);
        // This is not an error condition, we just loaded defaults
        err = ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading volatile data from NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Volatile data loaded successfully from NVS.");
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Save static data to NVS
 */
esp_err_t nvs_manager_save_static_data(const static_data_t *data) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, KEY_STATIC_DATA, data, sizeof(static_data_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing static data to NVS: %s", esp_err_to_name(err));
    } else {
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error committing static data to NVS: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Static data saved successfully.");
        }
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Load static data from NVS
 */
esp_err_t nvs_manager_load_static_data(static_data_t *data) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS namespace '%s' not found. Loading defaults.", NVS_NAMESPACE);
        load_static_defaults(data);
        return ESP_ERR_NVS_NOT_FOUND; // Return specific error to signal defaults loaded
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        load_static_defaults(data); // Load defaults on any other error for safety
        return err;
    }

    size_t required_size = sizeof(static_data_t);
    err = nvs_get_blob(nvs_handle, KEY_STATIC_DATA, data, &required_size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Static data not found in NVS. Loading defaults.");
        load_static_defaults(data);
        // This is not an error condition, we just loaded defaults
        err = ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error reading static data from NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Static data loaded successfully from NVS.");
    }

    nvs_close(nvs_handle);
    return err;
}
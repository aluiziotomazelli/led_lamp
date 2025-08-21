/**
 * @file espnow_controller.c
 * @brief ESP-NOW communication controller implementation
 * 
 * @details This file implements ESP-NOW based communication between
 * master and slave devices for synchronized LED control.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <string.h>

// ESP-IDF WiFi and ESP-NOW
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"

// ESP-IDF system services
#define LOG_LOCAL_LEVEL ESP_LOG_INFO  // ✅ Must come before esp_log.h
#include "esp_log.h"

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Project specific headers
#include "espnow_controller.h"
#include "input_integrator.h"
#include "project_config.h"

static const char *TAG = "ESPNOW_CTRL";
static QueueHandle_t q_espnow_events = NULL;

#if IS_MASTER
static bool master_enabled = true;  ///< Master sending enable flag
#endif

/**
 * @brief Callback function for when data is sent
 * 
 * @param[in] mac_addr MAC address of the recipient
 * @param[in] status Send status (success or failure)
 * 
 * @note This callback runs in ISR context
 */
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "Message sent successfully to " MACSTR, MAC2STR(mac_addr));
    } else {
        ESP_LOGE(TAG, "Failed to send message to " MACSTR, MAC2STR(mac_addr));
    }
}

/**
 * @brief Callback function for when data is received
 * 
 * @param[in] recv_info Reception information including source MAC
 * @param[in] incoming_data Received data buffer
 * @param[in] len Length of received data
 * 
 * @note This callback runs in ISR context
 * @warning Only active when compiled as SLAVE
 */
static void on_data_recv(const esp_now_recv_info_t *recv_info, const uint8_t *incoming_data, int len) {
#if IS_SLAVE
    const uint8_t *mac_addr = recv_info->src_addr;
    if (mac_addr == NULL || incoming_data == NULL || len <= 0) {
        return;
    }

    if (len != sizeof(espnow_message_t)) {
        ESP_LOGW(TAG, "Received message of incorrect size (%d) from " MACSTR, len, MAC2STR(mac_addr));
        return;
    }

    espnow_event_t evt;
    memcpy(evt.mac_addr, mac_addr, sizeof(evt.mac_addr));
    memcpy(&evt.msg, incoming_data, sizeof(espnow_message_t));

    if (xQueueSend(q_espnow_events, &evt, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to send received ESP-NOW event to queue.");
    }
#endif
}

/**
 * @brief Initialize Wi-Fi in STA mode for ESP-NOW
 * 
 * @note STA mode is required for ESP-NOW communication
 * @warning Must be called before espnow_controller_init
 */
static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Wi-Fi initialized in STA mode");
}

/**
 * @brief Initialize the ESP-NOW controller
 * 
 * @param[in] espnow_queue The queue to which received ESP-NOW events will be posted
 * 
 * @note This function sets up both master and slave configurations based on compile flags
 * @warning ESP-NOW must be enabled in project configuration
 */
void espnow_controller_init(QueueHandle_t espnow_queue) {
#if ESP_NOW_ENABLED
    q_espnow_events = espnow_queue;

    // Initialize WiFi and ESP-NOW
    wifi_init();
    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGI(TAG, "ESP-NOW initialized");

    // Register callbacks
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

#if IS_MASTER
    ESP_LOGI(TAG, "Running as MASTER. Adding %d slaves", num_slaves);
    for (int i = 0; i < num_slaves; ++i) {
        esp_now_peer_info_t peer_info = {0};
        memcpy(peer_info.peer_addr, slave_mac_addresses[i], ESP_NOW_ETH_ALEN);
        peer_info.channel = 0;
        peer_info.encrypt = false;
        
        if (esp_now_add_peer(&peer_info) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add slave peer " MACSTR, MAC2STR(slave_mac_addresses[i]));
        } else {
            ESP_LOGI(TAG, "Added slave peer: " MACSTR, MAC2STR(slave_mac_addresses[i]));
        }
    }
#endif // IS_MASTER

#if IS_SLAVE
    ESP_LOGI(TAG, "Running as SLAVE");
#endif // IS_SLAVE

#endif // ESP_NOW_ENABLED
}

/**
 * @brief Send an ESP-NOW message to all registered slave peers
 * 
 * @param[in] msg The message to send
 * 
 * @note Only works when compiled as MASTER with ESP_NOW_ENABLED
 * @warning Master sending must be enabled for messages to be sent
 */
void espnow_controller_send(const espnow_message_t *msg) {
#if ESP_NOW_ENABLED && IS_MASTER
    if (!master_enabled) {
        return; // Master sending is disabled
    }
    
    esp_err_t result = esp_now_send(NULL, (uint8_t *)msg, sizeof(espnow_message_t));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send broadcast message: %s", esp_err_to_name(result));
    }
#endif
}

/**
 * @brief Enable or disable the master's sending functionality
 * 
 * @param[in] enabled True to enable sending, false to disable
 * 
 * @note Only effective when compiled as MASTER
 */
void espnow_controller_set_master_enabled(bool enabled) {
#if IS_MASTER
    master_enabled = enabled;
    ESP_LOGI(TAG, "Master sending %s", enabled ? "enabled" : "disabled");
#endif
}

/**
 * @brief Check if the master's sending functionality is currently enabled
 * 
 * @return True if sending is enabled, false otherwise
 * 
 * @note Always returns false when compiled as SLAVE
 */
bool espnow_controller_is_master_enabled(void) {
#if IS_MASTER
    return master_enabled;
#else
    return false;
#endif
}
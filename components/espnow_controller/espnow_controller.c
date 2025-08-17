#include "espnow_controller.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "input_integrator.h"
#include "project_config.h"
#include <string.h>

static const char *TAG = "ESPNOW_CTRL";
static QueueHandle_t q_espnow_events = NULL;

// Callback function for when data is sent
static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "Message sent successfully to " MACSTR, MAC2STR(mac_addr));
    } else {
        ESP_LOGE(TAG, "Failed to send message to " MACSTR, MAC2STR(mac_addr));
    }
}

// Callback function for when data is received
static void on_data_recv(const uint8_t *mac_addr, const uint8_t *incoming_data, int len) {
#if IS_SLAVE
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

// Initialize Wi-Fi in STA mode
static void wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi initialized in STA mode.");
}

void espnow_controller_init(QueueHandle_t espnow_queue) {
#if ESP_NOW_ENABLED
    q_espnow_events = espnow_queue;

    wifi_init();

    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGI(TAG, "ESP-NOW initialized.");

    ESP_ERROR_CHECK(esp_now_register_send_cb(on_data_sent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(on_data_recv));

#if IS_MASTER
    ESP_LOGI(TAG, "Running as MASTER. Adding %d slaves.", num_slaves);
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
    ESP_LOGI(TAG, "Running as SLAVE.");
#endif // IS_SLAVE

#endif // ESP_NOW_ENABLED
}

void espnow_controller_send(const espnow_message_t *msg) {
#if ESP_NOW_ENABLED && IS_MASTER
    esp_err_t result = esp_now_send(NULL, (uint8_t *)msg, sizeof(espnow_message_t));
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send broadcast message: %s", esp_err_to_name(result));
    }
#endif
}

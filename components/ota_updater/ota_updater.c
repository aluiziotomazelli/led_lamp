/**
 * @file ota_updater.c
 * @brief OTA update implementation with SoftAP and HTTP server
 * 
 * @details This file implements Over-the-Air firmware updates using ESP32's
 *          SoftAP mode and embedded HTTP server for firmware file upload.
 * 
 * @author Your Name
 * @date 2024-03-15
 * @version 1.0
 */

// System includes
#include <string.h>

// FreeRTOS components
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// ESP-IDF system services
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"

// LWIP networking
#include "lwip/ip4_addr.h"

// Project specific headers
#include "ota_updater.h"
#include "nvs_manager.h"
#include "fsm.h"

// External queue for LED command feedback
extern QueueHandle_t led_cmd_queue;

static const char *TAG = "OTA_UPDATER_AP";

/**
 * @brief Macro to get minimum of two values
 */
#define MIN(a,b) (((a)<(b))?(a):(b))

// Forward declarations for URI handlers
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t update_post_handler(httpd_req_t *req);

// HTTP server instance
static httpd_handle_t server = NULL;

/**
 * @brief Root URI handler configuration
 */
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

/**
 * @brief Update URI handler configuration
 */
static const httpd_uri_t update = {
    .uri       = "/update",
    .method    = HTTP_POST,
    .handler   = update_post_handler,
    .user_ctx  = NULL
};

/**
 * @brief Starts the embedded web server
 * 
 * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
 * 
 * @note Configures and starts HTTP server with URI handlers
 */
static esp_err_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &update);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return ESP_FAIL;
}

/**
 * @brief Initializes Wi-Fi in SoftAP mode with static IP
 * 
 * @note Configures access point with predefined SSID and password
 *       Sets static IP address 192.168.4.1 for the AP interface
 */
static void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *p_netif = esp_netif_create_default_wifi_ap();
    assert(p_netif);
    
    // Stop DHCP server to set a static IP
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(p_netif));

    // Assign a static IP address to the AP interface
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1); // Gateway is the ESP itself
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(p_netif, &ip_info));

    // Start DHCP server
    ESP_ERROR_CHECK(esp_netif_dhcps_start(p_netif));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "ESP32_Updater",
            .ssid_len = strlen("ESP32_Updater"),
            .password = "password",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen((char*)wifi_config.ap.password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s",
             "ESP32_Updater", "password");
}

/**
 * @brief Embedded HTML for the OTA update web page
 * 
 * @note Contains form for file upload and JavaScript for AJAX submission
 */
const char* update_page_html =
    "<!DOCTYPE html><html><head><title>ESP32 OTA Update</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>"
    "<body><h1>ESP32 Firmware Update</h1>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='update' accept='.bin'>"
    "<input type='submit' value='Update'>"
    "</form>"
    "<p id='status'></p>"
    "<script>"
    "document.querySelector('form').addEventListener('submit', function(e) {"
    "e.preventDefault();"
    "var form = e.target;"
    "var fileInput = form.querySelector('input[type=\\'file\\']');"
    "var file = fileInput.files[0];"
    "if (!file) {"
    "  alert('Please select a file!');"
    "  return;"
    "}"
    "var status = document.getElementById('status');"
    "status.innerHTML = 'Uploading and updating... Please wait.';"
    "var xhr = new XMLHttpRequest();"
    "xhr.open('POST', form.action, true);"
    "xhr.onload = function() {"
    "  if (xhr.status === 200) {"
    "    status.innerHTML = 'Update successful! Device is rebooting.';"
    "  } else {"
    "    status.innerHTML = 'Update failed: ' + xhr.responseText;"
    "  }"
    "};"
    "xhr.send(file);"
    "});"
    "</script></body></html>";

/**
 * @brief Handler for root GET requests
 * 
 * @param req HTTP request structure
 * @return esp_err_t ESP_OK on success
 * 
 * @note Serves the embedded HTML update page
 */
static esp_err_t root_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Serving root page.");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, update_page_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/**
 * @brief Handler for firmware update POST requests
 * 
 * @param req HTTP request structure
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 * 
 * @note Handles firmware binary upload, writes to OTA partition,
 *       validates, and sets boot partition before rebooting
 */
static esp_err_t update_post_handler(httpd_req_t *req) {
    char buf[1024];
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;
    int remaining = req->content_len;
    int binary_file_length = 0;

    ESP_LOGI(TAG, "Starting OTA update...");

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Failed to get update partition");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%"PRIu32"",
             update_partition->subtype, update_partition->address);

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    while (remaining > 0) {
        int recv_len = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)));
        if (recv_len <= 0) {
            if (recv_len == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "File reception failed!");
            esp_ota_abort(update_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }

        err = esp_ota_write(update_handle, (const void *)buf, recv_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed (%s)!", esp_err_to_name(err));
            esp_ota_abort(update_handle);
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        binary_file_length += recv_len;
        ESP_LOGD(TAG, "Written %d bytes", binary_file_length);
        remaining -= recv_len;
    }

    ESP_LOGI(TAG, "Total Write binary data length: %d", binary_file_length);

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
        }
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA Update successful! Rebooting...");
    httpd_resp_send(req, "OTA Update successful! Rebooting...", HTTPD_RESP_USE_STRLEN);

    // Give the browser a moment to receive the response before restarting
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

    return ESP_OK;
}

/**
 * @brief Starts the SoftAP OTA update process
 * 
 * @return esp_err_t ESP_OK on success, ESP_FAIL on failure
 * 
 * @note Clears OTA flag, initializes Wi-Fi in AP mode, starts web server,
 *       and provides visual feedback via LED queue
 */
esp_err_t ota_updater_start(void) {
    ESP_LOGI(TAG, "Starting SoftAP OTA Updater...");

    // Send red feedback to the user
    if (led_cmd_queue != NULL) {
        led_command_t ota_feedback_cmd = {
            .cmd = LED_CMD_FEEDBACK_RED,
            .timestamp = 0, // Timestamp is not critical for feedback
            .value = 0,
            .param_idx = 0
        };
        xQueueSend(led_cmd_queue, &ota_feedback_cmd, 0); // Use 0 wait time, it's not critical if it fails
    }

    // 1. Clear the OTA flag first, as per user's robust error-handling suggestion
    ota_data_t ota_data;
    esp_err_t err = nvs_manager_load_ota_data(&ota_data);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to load OTA data to clear it. Error: %s", esp_err_to_name(err));
        // Continue anyway, as we want to start the AP if possible
    }

    ota_data.ota_mode_enabled = false;
    err = nvs_manager_save_ota_data(&ota_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to clear OTA flag in NVS. Error: %s", esp_err_to_name(err));
        // This is not fatal, but the device might re-enter OTA mode on next boot if manually reset.
    } else {
        ESP_LOGI(TAG, "OTA flag cleared successfully.");
    }

    // 2. Initialize Wi-Fi in AP mode
    wifi_init_softap();

    // 3. Start the web server
    err = start_webserver();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start webserver.");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA Updater started successfully. Connect to SSID 'ESP32_Updater' and navigate to 192.168.4.1");
    return ESP_OK;
}
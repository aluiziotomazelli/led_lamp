#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"

#include "ota_updater.h"
#include "nvs_manager.h"

static const char *TAG = "OTA_UPDATER_AP";

// Forward declarations for URI handlers
static esp_err_t root_get_handler(httpd_req_t *req);
static esp_err_t update_post_handler(httpd_req_t *req);

// HTTP server instance
static httpd_handle_t server = NULL;

// URI Handlers
static const httpd_uri_t root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t update = {
    .uri       = "/update",
    .method    = HTTP_POST,
    .handler   = update_post_handler,
    .user_ctx  = NULL
};


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

static void wifi_init_softap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *p_netif = esp_netif_create_default_wifi_ap();
    assert(p_netif);

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

// Embedded HTML for the update page
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
    "var data = new FormData(form);"
    "var status = document.getElementById('status');"
    "status.innerHTML = 'Uploading and updating... Please wait.';"
    "var xhr = new XMLHttpRequest();"
    "xhr.open('POST', form.action, true);"
    "xhr.onload = function() {"
    "if (xhr.status === 200) {"
    "status.innerHTML = 'Update successful! Device is rebooting.';"
    "} else {"
    "status.innerHTML = 'Update failed: ' + xhr.responseText;"
    "}"
    "};"
    "xhr.send(data);"
    "});"
    "</script></body></html>";

static esp_err_t root_get_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Serving root page.");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, update_page_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

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
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%x",
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


esp_err_t ota_updater_start(void) {
    ESP_LOGI(TAG, "Starting SoftAP OTA Updater...");

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

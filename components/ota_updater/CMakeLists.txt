idf_component_register(SRCS "ota_updater.c"
                    INCLUDE_DIRS "include"
                    REQUIRES nvs_manager esp_https_ota esp_http_client esp_wifi nvs_flash esp_event esp_log)

idf_component_register(SRCS "encoder.c"
                    INCLUDE_DIRS "include"
                    REQUIRES freertos
                             esp_timer
                             esp_driver_gpio)

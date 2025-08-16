idf_component_register(SRCS "switch.c"
                    INCLUDE_DIRS "include"
                    REQUIRES	freertos
                    			esp_driver_gpio
                    			shared
                    			)

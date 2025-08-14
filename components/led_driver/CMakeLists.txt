idf_component_register(SRCS "led_driver.c"
                    INCLUDE_DIRS "include"
                    REQUIRES 		shared 
                    				led_strip
                    				led_controller
                    PRIV_REQUIRES
#                        			spi_driver
                        			esp_common # For esp_err_t and ESP_LOG
                    				)

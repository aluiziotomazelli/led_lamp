idf_component_register(SRCS "led_driver.c"
                    INCLUDE_DIRS "include"
                    REQUIRES 	shared 
                    			led_strip
                    			led_controller
                    			)

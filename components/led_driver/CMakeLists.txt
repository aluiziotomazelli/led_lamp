idf_component_register(SRCS "led_driver.cpp"
                    INCLUDE_DIRS "include"
                    REQUIRES shared FastLED-idf led_controller)

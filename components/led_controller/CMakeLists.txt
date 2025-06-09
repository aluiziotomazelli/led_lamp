idf_component_register(SRCS "led_controller.c"
                    INCLUDE_DIRS "include"
                    REQUIRES
                        led_strip
                    PRIV_REQUIRES
#                        spi_driver
                        esp_common # For esp_err_t and ESP_LOG
#                    TEST_SRCS test/test_led_controller.c
                    # This ensures that when building tests for this component,
                    # test_led_controller.c is compiled and linked.
                    # The ESP-IDF build system automatically handles linking with Unity framework.
                    # It also typically defines a main function for the test app if one isn't
                    # explicitly created in the test source files (though test_led_controller.c has one).
                    # For component tests, a per-component test app is usually built.
                        )

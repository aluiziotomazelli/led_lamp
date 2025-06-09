idf_component_register(SRCS "led_effects.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_common # For basic types if not pulled by others
                       # PRIV_REQUIRES cJSON # Add if cJSON is used later
#                       TEST_SRCS test/test_led_effects.c
                       )

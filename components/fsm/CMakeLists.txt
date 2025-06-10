idf_component_register(SRCS "fsm.c"
                    INCLUDE_DIRS "include"
					PRIV_REQUIRES
					    input_integrator
						led_controller
						led_effects
						esp_common # For ESP_LOG, esp_err_t etc.
#                    TEST_SRCS test/test_fsm.c
                    )

# TODO NVS: When NVS is fully implemented, add 'nvs_flash' to PRIV_REQUIRES.
# Example:
# idf_component_register(SRCS "fsm.c"
# INCLUDE_DIRS "include"
# PRIV_REQUIRES
#     input_integrator
#     led_controller
#     led_effects
#     esp_common
#     nvs_flash
# )

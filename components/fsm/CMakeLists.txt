idf_component_register(SRCS "fsm.c"
                    INCLUDE_DIRS "include"
					REQUIRES input_integrator 
#							 effects
							 led_controller
                             shared)

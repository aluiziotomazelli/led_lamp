idf_component_register(SRCS "espnow_controller.c"
                    INCLUDE_DIRS "include"
                    REQUIRES 	freertos 
                    			esp_wifi 
                    			#esp_now 
                    			#esp_log 
                    			fsm 
                    			input_integrator 
                    			)

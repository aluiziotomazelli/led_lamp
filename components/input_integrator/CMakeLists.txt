idf_component_register(SRCS "input_integrator.c"
                    INCLUDE_DIRS "include"
                    REQUIRES 	freertos 
                    			button 
                    			encoder 
                    			shared 
                    			touch
                    			switch
                    			espnow_controller
                    			)



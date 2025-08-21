#==============================================================================
# ESP-NOW Controller Component
#
# Description: ESP-NOW communication controller for master/slave LED control
#              synchronization across multiple devices
#
# Author: Your Name
# Date: 2024-03-15
# Version: 1.0
#==============================================================================

#------------------------------------------------------------------------------
# COMPONENT REGISTRATION
#------------------------------------------------------------------------------

# Register this component with the ESP-IDF build system
idf_component_register(
    # Source files for this component
    SRCS "espnow_controller.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS "include"
    
    # Required components (dependencies)
    REQUIRES 
        freertos           # FreeRTOS for queues and task management
        esp_wifi           # WiFi stack for ESP-NOW communication
        fsm                # Finite state machine for LED control
        input_integrator   # Input processing and integration
)

#------------------------------------------------------------------------------
# COMPONENT CONFIGURATION (Optional)
#------------------------------------------------------------------------------

# Uncomment to set component-specific compile options
# set(COMPONENT_ADD_INCLUDEDIRS "private_include")
# set(COMPONENT_PRIV_REQUIRES some_private_dependency)

#==============================================================================
# END OF FILE
#==============================================================================
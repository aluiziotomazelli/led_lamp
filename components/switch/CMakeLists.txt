#==============================================================================
# Switch Driver Component
#
# Description: Switch driver with hardware debouncing and event queuing
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
    SRCS "switch.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS "include"
    
    # Required components (dependencies)
    REQUIRES 
        freertos           # FreeRTOS for tasks, queues, and notifications
        esp_driver_gpio    # GPIO driver for switch pin control and interrupts
        shared             # Shared project utilities and configuration
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
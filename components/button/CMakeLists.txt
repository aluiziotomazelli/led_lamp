#==============================================================================
# Button Driver Component
#
# Description: Button driver with advanced click detection (single, double, long)
#              using finite state machine and debouncing
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
    SRCS "button.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS "include"
    
    # Required components (dependencies)
    REQUIRES 
        esp_driver_gpio      # GPIO driver for button pin control
        esp_timer            # Timer services for timing measurements
        freertos             # FreeRTOS for tasks, queues, and notifications
        driver               # ESP32 driver framework
        shared               # Shared project utilities and definitions
)

#------------------------------------------------------------------------------
# COMPONENT CONFIGURATION (Optional - can be added if needed)
#------------------------------------------------------------------------------

# Example of additional configuration options:
# set(COMPONENT_ADD_INCLUDEDIRS "private_include")
# set(COMPONENT_PRIV_REQUIRES some_private_dependency)
# set(COMPONENT_EMBED_TXTFILES "config/button_defaults.txt")

#==============================================================================
# END OF FILE
#==============================================================================
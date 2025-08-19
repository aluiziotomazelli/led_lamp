#==============================================================================
# LED Controller Component
#
# Description: High-level LED strip controller with animation support
#              and color management
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
    SRCS "led_controller.c" "led_effects.c" "candle_math.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS "include"
    
    # Required components (dependencies)
    REQUIRES 
        fsm                 # Finite State Machine for command processing
        esp_timer           # Timer services for timing measurements
        freertos            # FreeRTOS for tasks, queues, and notifications
        driver              # ESP32 driver framework
        shared              # Shared project utilities and definitions
        nvs_manager         # Non-volatile storage management
)

#------------------------------------------------------------------------------
# COMPONENT CONFIGURATION (Optional - can be added if needed)
#------------------------------------------------------------------------------

# Example of additional configuration options:
# set(COMPONENT_ADD_INCLUDEDIRS "private_include")
# set(COMPONENT_PRIV_REQUIRES some_private_dependency)
# set(COMPONENT_EMBED_TXTFILES "config/controller_defaults.txt")

#==============================================================================
# END OF FILE
#==============================================================================
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
    SRCS
        "led_controller.c"
        "led_effects.c"
        "effects/breathing.c"
        "effects/candle.c"
        "effects/candle_math.c"
        "effects/candle_math_logic.c"
        "effects/christmas_tree.c"
        "effects/christmas_twinkle.c"
        "effects/random_twinkle.c"
        "effects/static_color.c"
        "effects/white_temp.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS
        "include"
        "effects/include"
    
    # Required components (dependencies)
    REQUIRES 
        fsm                 # Finite State Machine for command processing
        led_driver          # LED driver for setting correction values
        esp_timer           # Timer services for timing measurements
        freertos            # FreeRTOS for tasks, queues, and notifications
        driver              # ESP32 driver framework
        nvs_manager         # Non-volatile storage management
        ota_updater
        shared              # Shared project utilities and definitions
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
#==============================================================================
# Touch Sensor Driver Component
#
# Description: Capacitive touch sensor driver with advanced gesture detection,
#              automatic recalibration, and debouncing
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
    SRCS "touch.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS "include"
    
    # Required components (dependencies)
    REQUIRES 
        esp_driver_gpio          # GPIO driver for general IO
        esp_timer                # Timer services for recalibration and timing
        freertos                 # FreeRTOS for tasks, queues, and notifications
        driver                   # ESP32 driver framework
        esp_driver_touch_sens    # Touch sensor hardware driver
        shared                   # Shared project utilities and configuration
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
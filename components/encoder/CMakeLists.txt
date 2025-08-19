#==============================================================================
# Rotary Encoder Driver Component
#
# Description: Quadrature encoder driver with state machine decoding,
#              half/full step modes, and acceleration support
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
    SRCS "encoder.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS "include"
    
    # Required components (dependencies)
    REQUIRES 
        freertos           # FreeRTOS for tasks, queues, and notifications
        esp_timer          # Timer services for timing measurements and acceleration
        esp_driver_gpio    # GPIO driver for encoder pin control and interrupts
        shared             # Shared project utilities and configuration
)

#------------------------------------------------------------------------------
# COMPONENT CONFIGURATION (Optional)
#------------------------------------------------------------------------------

# Uncomment to set component-specific compile options
# set(COMPONENT_ADD_INCLUDEDIRS "private_include")
# set(COMPONENT_PRIV_REQUIRES some_private_dependency)

# Uncomment to add component-specific configuration options
# set(COMPONENT_ADD_LDFLAGS -Wl,--wrap=some_function)

#==============================================================================
# END OF FILE
#==============================================================================
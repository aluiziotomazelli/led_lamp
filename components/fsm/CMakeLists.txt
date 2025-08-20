#==============================================================================
# Finite State Machine Component
#
# Description: Main state machine for LED control system with support for
#              multiple modes, timeout handling, and input event processing
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
    SRCS "fsm.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS "include"
    
    # Required components (dependencies)
    REQUIRES 
        input_integrator   # Input event integration system
        led_controller     # LED strip control and effects
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
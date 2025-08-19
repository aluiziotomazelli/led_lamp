#==============================================================================
# Input Integrator Component
#
# Description: Input event integration system that combines events from
#              multiple input sources into a single unified event stream
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
    SRCS "input_integrator.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS "include"
    
    # Required components (dependencies)
    REQUIRES 
        freertos           # FreeRTOS for queues, tasks, and queue sets
        button             # Button driver component
        encoder            # Rotary encoder driver component
        shared             # Shared project utilities and configuration
        touch              # Touch sensor driver component
        switch             # Switch driver component
        espnow_controller  # ESP-NOW communication controller
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
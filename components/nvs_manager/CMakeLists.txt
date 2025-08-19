#==============================================================================
# NVS Manager Component
#
# Description: Non-volatile storage management for LED configuration data
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
    SRCS "nvs_manager.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS "include"
    
    # Required components (dependencies)
    REQUIRES 
        nvs_flash          # Non-volatile storage flash library
        led_controller     # LED controller for effect parameter definitions
)

#------------------------------------------------------------------------------
# COMPONENT CONFIGURATION (Optional - can be added if needed)
#------------------------------------------------------------------------------

# Example of additional configuration options:
# set(COMPONENT_ADD_INCLUDEDIRS "private_include")
# set(COMPONENT_PRIV_REQUIRES some_private_dependency)
# set(COMPONENT_EMBED_TXTFILES "config/nvs_defaults.txt")

#==============================================================================
# END OF FILE
#==============================================================================
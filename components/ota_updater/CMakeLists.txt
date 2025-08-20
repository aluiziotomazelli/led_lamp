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
        "ota_updater.c"
    
    # Public include directories (visible to other components)
    INCLUDE_DIRS
        "include"
    
    # Required components (dependencies)
    REQUIRES 
        nvs_manager         # Non-volatile storage management
    	esp_http_server 
    	esp_wifi 
    	nvs_flash 
    	esp_event
    	app_update
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
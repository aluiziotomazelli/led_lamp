#==============================================================================
# OTA Updater Component
#
# Description: Over-the-Air firmware update functionality via SoftAP
#              with embedded web server for firmware upload
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
    	esp_http_server     # HTTP server for OTA updates
    	esp_wifi            # Wi-Fi functionality for SoftAP
    	nvs_flash           # Non-volatile storage
    	esp_event           # Event loop handling
    	app_update          # Application update functionality
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
# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS main.c         # list the source files of this component
    INCLUDE_DIRS        "."
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            shared button encoder input_integrator fsm
    PRIV_REQUIRES       # optional, list the private requirements
    
    
    
)

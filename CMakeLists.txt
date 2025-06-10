# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
#cmake_minimum_required(VERSION 3.5)
#
#include($ENV{IDF_PATH}/tools/cmake/project.cmake)
#project(app-template)
#
#set(EXTRA_COMPONENT_DIRS 
#			components/button 
#			components/input_manager 
#			components/encoder)
#			
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/shared_config/include)		


	
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Lista TODOS os componentes, incluindo o shared
set(COMPONENTS 
    main
    shared
    button
    encoder
    input_integrator
    led_effects
    led_controller
    led_renderer
    fsm
)

# Configuração definitiva
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/components/button
    ${CMAKE_CURRENT_SOURCE_DIR}/components/input_integrator
    ${CMAKE_CURRENT_SOURCE_DIR}/components/encoder
    ${CMAKE_CURRENT_SOURCE_DIR}/components/led_effects
    ${CMAKE_CURRENT_SOURCE_DIR}/components/led_controller
    ${CMAKE_CURRENT_SOURCE_DIR}/components/led_renderer
    ${CMAKE_CURRENT_SOURCE_DIR}/components/fsm
)

project(app-template)
cmake_minimum_required(VERSION 4.0)
project(Charm-os)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_library(${target_name})

#add_compile_options(
target_compile_options(${target_name} PRIVATE
        -Wall
#        -fno-exceptions
#        -fno-rtti
        #        -ffreestanding
        #        -nostdlib
)


file(GLOB_RECURSE MODULE_INTERFACE_UNITS "Modules/*.cppm")
target_sources(${target_name}
        PUBLIC
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Modules"
        FILES ${MODULE_INTERFACE_UNITS})

message(STATUS "${target_name} Draft Examples: Windows -> ${CMAKE_CURRENT_SOURCE_DIR}/Draft/Examples/windows")
message(STATUS "${target_name} Draft Examples: STM32   -> ${CMAKE_CURRENT_SOURCE_DIR}/Draft/Examples/stm32f103c8")

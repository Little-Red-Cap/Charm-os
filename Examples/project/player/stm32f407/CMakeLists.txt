cmake_minimum_required(VERSION 4.0)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME f407vg)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})

# Enable CMake support for ASM and C languages
enable_language(C ASM CXX)

message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
file(GLOB_RECURSE MODULE_INTERFACE_UNITS
        "../app/*.cppm"
        "./*.cppm"
)
list(REMOVE_ITEM MODULE_INTERFACE_UNITS
    "${CMAKE_CURRENT_SOURCE_DIR}/audio_player_demo.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/audio_demo.cppm"
)
list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "audio_player_demo\\.cppm$")
list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "audio_demo\\.cppm$")
list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "ui_tile_backend\\.cppm$")

target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
    # Add user sources here
        Core/Src/main.cpp

    PUBLIC
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../"
    FILES
        ${MODULE_INTERFACE_UNITS}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
    CHARM_VIVID_SCREEN_WIDTH=480
    CHARM_VIVID_SCREEN_HEIGHT=800
    CHARM_VIVID_SCREEN_PIXEL_FORMAT_RGB565
    CHARM_VIVID_ENABLE_LAYER_CACHE=0
    CHARM_VIVID_LAYER_CACHE_WIDTH=1
    CHARM_VIVID_LAYER_CACHE_HEIGHT=1
    CHARM_VIVID_LAYER_CACHE_SLOTS=1
    CHARM_AUDIO_SINK_I2S=1
    CHARM_AUDIO_USE_VFS=1
    CHARM_AUDIO_ENABLE_STRESS=0
    CHARM_AUDIO_MAX_PERIOD_FRAMES=256
    CHARM_AUDIO_MAX_CHUNK_MULT=4
    CHARM_AUDIO_MAX_FIFO_MS=70
)

# Ensure Charm-os is built with the same feature flags.
add_compile_definitions(CHARM_VIVID_ENABLE_LAYER_CACHE=0)
add_compile_definitions(CHARM_AUDIO_SINK_I2S=1)
add_compile_definitions(CHARM_AUDIO_USE_VFS=1)
add_compile_definitions(CHARM_AUDIO_ENABLE_STRESS=0)
add_compile_definitions(CHARM_AUDIO_MAX_PERIOD_FRAMES=256)
add_compile_definitions(CHARM_AUDIO_MAX_CHUNK_MULT=4)
add_compile_definitions(CHARM_AUDIO_MAX_FIFO_MS=70)
add_compile_definitions(STM32F407xx)
add_compile_definitions(USE_HAL_DRIVER)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_FATFS ON CACHE BOOL "" FORCE)
set(CHARM_AUDIO_SINK_I2S ON CACHE BOOL "" FORCE)
set(CHARM_FATFS_ROOT "${CHARM_ROOT}/Modules/thirdparty/fatfs" CACHE PATH "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")

# Provide CubeMX headers to Charm-os so I2S sink can include i2s.h/main.h.
target_include_directories(Charm-os PRIVATE
    $<TARGET_PROPERTY:stm32cubemx,INTERFACE_INCLUDE_DIRECTORIES>
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    Charm-os

    # Add user defined libraries
)

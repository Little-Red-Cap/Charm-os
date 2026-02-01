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
set(CMAKE_PROJECT_NAME vivid-example-stm32)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(CHARM_MCU_KERNEL_DEMO "Use Charm kernel demo application" OFF)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM CXX)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)


set(CMAKE_CXX_FLAGS_DEBUG "-Og -g3" CACHE STRING "" FORCE)
#string(APPEND CMAKE_CXX_FLAGS " -fno-rtti -fno-exceptions -fmodules-ts")
#target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
#        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -fno-exceptions -fmodules-ts>
#)

file(GLOB_RECURSE MODULE_INTERFACE_UNITS
        "../app/*.cppm"
        "./*.cppm"
)
# Add sources to executable
if(CHARM_MCU_KERNEL_DEMO)
    set(APP_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/main_mcu_stub.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/kernel.port.stm32.cpp"
    )
else()
    set(APP_SOURCES
        Core/Src/application.cpp
    )
endif()

target_sources(${CMAKE_PROJECT_NAME}
        PRIVATE
    # Add user sources here
            ${APP_SOURCES}

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
        LOG_LEVEL_DEBUG
        OUT_ENABLE_BINARY
        OUT_ENABLE_FLOAT
)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../.." "${CMAKE_BINARY_DIR}/Charm-ink")
#target_compile_options(Charm-ink PRIVATE
#        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -fno-exceptions -fmodules-ts>
#)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    Charm-ink
    # Add user defined libraries
)


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

message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/product_player_identity.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform_stm32h747_hal.cmake")

# Set the project name
set(CMAKE_PROJECT_NAME stn32h747_hqzy_CM7)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})
platform_stm32h747_hal_attach_target(${CMAKE_PROJECT_NAME})

# Enable CMake support for ASM and C languages
enable_language(C ASM CXX)

# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true") 

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/target_player_base.cmake")

message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_PRODUCT: ${PLAYER_PRODUCT}")
message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_PLATFORM: ${PLAYER_PLATFORM}")
message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_BOARD: ${PLAYER_BOARD}")
message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_SCENARIO: ${PLAYER_SCENARIO}")
message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_PROFILE: ${PLAYER_PROFILE}")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${PLAYER_PLATFORM_COMMON_SOURCES}
    ${PLAYER_PROFILE_SOURCE}
    ${PLAYER_PROFILE_IMPL_SOURCES}
)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/board_hqzy_cm7_modules.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/product_player_modules.cmake")

set(MODULE_INTERFACE_UNITS)
foreach(module_group IN LISTS PLAYER_PROFILE_MODULE_GROUPS)
    list(APPEND MODULE_INTERFACE_UNITS ${${module_group}})
endforeach()
list(REMOVE_DUPLICATES MODULE_INTERFACE_UNITS)

target_sources(${CMAKE_PROJECT_NAME}
    PUBLIC
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.."
    FILES
        ${MODULE_INTERFACE_UNITS}
)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined libraries
)


add_compile_definitions(STM32H747xx)
add_compile_definitions(CORE_CM7)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_FATFS ON CACHE BOOL "" FORCE)
set(CHARM_FATFS_ROOT "${CHARM_ROOT}/Modules/thirdparty/fatfs" CACHE PATH "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")

target_compile_definitions(Charm-os PRIVATE CHARM_KERNEL_REQUIRE_SSU_META=1)
target_include_directories(Charm-os PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Inc
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/CMSIS/Device/ST/STM32H7xx/Include
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/CMSIS/Include
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    Charm-os
)

# Validate that STM32CubeMX code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()

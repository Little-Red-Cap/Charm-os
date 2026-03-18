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

# Compiler options
set(STM32_MCU_FLAGS  "-mcpu=cortex-m7 -mfpu=fpv5-d16 -mfloat-abi=hard " )

#Linker options
#set(STM32_LINKER_SCRIPT stm32h747xx_flash_CM7.ld)
set(STM32_LINKER_SCRIPT STM32H747XIHX_FLASH.ld)
#set(STM32_LINKER_SCRIPT STM32H747XIHX_RAM.ld)
set(STM32_LINKER_OPTION  )

# Include toolchain file
include("../gcc-arm-none-eabi.cmake")

# Set the project name
set(CMAKE_PROJECT_NAME stn32h747_hqzy_CM7)
project(${CMAKE_PROJECT_NAME})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Include mx-generated 
include("mx-generated.cmake")

# Enable CMake support for ASM and C languages
enable_language(C ASM CXX)

# In order to use CMake for cross-compiling
set(CMAKE_CXX_COMPILER_FORCED "true") 

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE 
    # Add user defined symbols
    CORE_CM7
    STM32H747xx
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Class/CompositeBuilder/Inc
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
#    app/main.cpp
    app/main-player.cpp
#    app/main-usb-as.cpp
    app/uart_bridge.c
    app/port_impl.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Class/CompositeBuilder/Src/usbd_composite_builder.c
    USB_DEVICE/Class/usbd_msc.c
    USB_DEVICE/Class/usbd_msc_bot.c
    USB_DEVICE/Class/usbd_msc_data.c
    USB_DEVICE/Class/usbd_msc_scsi.c
    USB_DEVICE/App/usbd_storage_if.c
)

set(MODULE_INTERFACE_UNITS
    "${CMAKE_CURRENT_SOURCE_DIR}/app/fs_demo_mmc.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/player_entry.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/usb_system.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/system_entry.cppm"
)
target_sources(${CMAKE_PROJECT_NAME}
    PUBLIC
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
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

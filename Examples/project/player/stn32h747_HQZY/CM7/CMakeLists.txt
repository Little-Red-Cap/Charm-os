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

set(PLAYER_PROFILE "USB_SELF_MSC" CACHE STRING
    "Player startup profile for stm32h747 HQZY CM7")
set_property(CACHE PLAYER_PROFILE PROPERTY STRINGS
    LEGACY_MAIN
    USB_AS
    USB_AUDIO
    USB_SELF_MSC
    USB_STORAGE)

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
    CHARM_ENTRY_ALLOWED=1`r`n    CHARM_KERNEL_REQUIRE_SSU_META=1
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Class/CompositeBuilder/Inc
)

# Add sources to executable
set(PLAYER_PROFILE_SOURCE "")
set(PLAYER_PROFILE_IMPL_SOURCES "")
set(PLAYER_PROFILE_MODULE_GROUPS "")

if (PLAYER_PROFILE STREQUAL "LEGACY_MAIN")
    set(PLAYER_PROFILE_SOURCE app/main.cpp)
    set(PLAYER_PROFILE_MODULE_GROUPS
        MODULE_GROUP_BASE
        MODULE_GROUP_FS
        MODULE_GROUP_AUDIO
        MODULE_GROUP_DISPLAY
        MODULE_GROUP_PLAYER_UI
        MODULE_GROUP_LEGACY_RUNTIME)
elseif (PLAYER_PROFILE STREQUAL "USB_AS")
    set(PLAYER_PROFILE_SOURCE app/main-usb-as.cpp)
    set(PLAYER_PROFILE_IMPL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/hqzy_cm7_usb_as.cpp")
    set(PLAYER_PROFILE_MODULE_GROUPS
        MODULE_GROUP_BASE
        MODULE_GROUP_FS
        MODULE_GROUP_USB_STORAGE_RUNTIME)
elseif (PLAYER_PROFILE STREQUAL "USB_AUDIO")
    set(PLAYER_PROFILE_SOURCE app/main-usb-audio.cpp)
    set(PLAYER_PROFILE_IMPL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/hqzy_cm7_usb_audio.cpp")
elseif (PLAYER_PROFILE STREQUAL "USB_SELF_MSC")
    set(PLAYER_PROFILE_SOURCE app/main-usb-self-msc.cpp)
    set(PLAYER_PROFILE_MODULE_GROUPS
        MODULE_GROUP_PLAYER_PROFILES
        MODULE_GROUP_HQZY_CM7_RUNTIME
        MODULE_GROUP_HQZY_CM7_PROFILE_SYSTEM)
elseif (PLAYER_PROFILE STREQUAL "USB_STORAGE")
    set(PLAYER_PROFILE_SOURCE app/main-usb-storage.cpp)
    set(PLAYER_PROFILE_IMPL_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/hqzy_cm7_usb_storage.cpp")
    set(PLAYER_PROFILE_MODULE_GROUPS
        MODULE_GROUP_BASE
        MODULE_GROUP_FS
        MODULE_GROUP_USB_STORAGE_RUNTIME)
else()
    message(FATAL_ERROR "Unsupported PLAYER_PROFILE: ${PLAYER_PROFILE}")
endif()

message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_PROFILE: ${PLAYER_PROFILE}")

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    app/profile_main.cpp
    ${PLAYER_PROFILE_SOURCE}
    ${PLAYER_PROFILE_IMPL_SOURCES}
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

set(MODULE_GROUP_BASE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_active.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_hqzy.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_caps.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_config.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_console.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_keys.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_sdmmc.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_usb.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/board_sdram.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/debug_hooks.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_config.cppm"
)

set(MODULE_GROUP_FS
    "${CMAKE_CURRENT_SOURCE_DIR}/app/fs_demo.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/fs_demo_mmc.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/fs_demo_sd.cppm"
)

set(MODULE_GROUP_AUDIO
    "${CMAKE_CURRENT_SOURCE_DIR}/app/audio_mp3_demo.cppm"
)

set(MODULE_GROUP_DISPLAY
    "${CMAKE_CURRENT_SOURCE_DIR}/app/display_st7305.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/ink_demo.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/st7305.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/st7305.panels.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../bsp/st7305.transform.cppm"
)

set(MODULE_GROUP_PLAYER_UI
    "${CMAKE_CURRENT_SOURCE_DIR}/../../app-ink/hqzy/player_app_state.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../app-ink/hqzy/player_controller.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../app-ink/hqzy/player_fs_utils.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../app-ink/hqzy/player_ui_ink.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/player_entry.cppm"
)

set(MODULE_GROUP_LEGACY_RUNTIME
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_boot_debug.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_boot_fs.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_init_graph.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_post_bringup.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_pre_bringup.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/app_run_modes.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/system_entry.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/app/usb_system.cppm"
)

set(MODULE_GROUP_USB_STORAGE_RUNTIME
    "${CMAKE_CURRENT_SOURCE_DIR}/app/usb_system.cppm"
)

set(MODULE_GROUP_PLAYER_PROFILES
    "${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/hqzy_cm7_usb_self_msc.cppm"
)

set(MODULE_GROUP_HQZY_CM7_RUNTIME
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/boot_log.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/board_platform.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/runtime_bringup.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/sdmmc_glue.cppm"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../runtime/hqzy_cm7/usb_glue.cppm"
)

set(MODULE_GROUP_HQZY_CM7_PROFILE_SYSTEM
    "${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/hqzy_cm7_usb_self_msc.system.cppm"
)

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




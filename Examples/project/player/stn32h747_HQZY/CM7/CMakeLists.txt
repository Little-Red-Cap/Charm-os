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

function(charm_configure_usb_audio_target target_name)
    add_executable(${target_name})

    target_compile_definitions(${target_name} PRIVATE
        CORE_CM7
        STM32H747xx
        CHARM_ENTRY_ALLOWED=1
        CHARM_KERNEL_REQUIRE_SSU_META=1
        USE_HAL_DRIVER
        USE_PWR_LDO_SUPPLY
        $<$<CONFIG:Debug>:DEBUG>
    )

    target_include_directories(${target_name} PRIVATE
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Class/CompositeBuilder/Inc
        ./Core/Inc
        ./USB_DEVICE/App
        ./USB_DEVICE/Target
        ./USB_DEVICE/Class
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Inc
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Inc
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Class/AUDIO/Inc
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/CMSIS/Device/ST/STM32H7xx/Include
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/CMSIS/Include
    )

    target_sources(${target_name} PRIVATE
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_core.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ctlreq.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Middlewares/ST/STM32_USB_Device_Library/Core/Src/usbd_ioreq.c
        ./USB_DEVICE/Target/usbd_conf.c
        ./USB_DEVICE/App/usb_device.c
        ./USB_DEVICE/App/usbd_desc.c
        ./USB_DEVICE/App/usbd_audio_if.c
        ./Core/Src/main.c
        ./Core/Src/gpio.c
        ./Core/Src/dma.c
        ./Core/Src/fmc.c
        ./Core/Src/i2s.c
        ./Core/Src/sdmmc.c
        ./Core/Src/spi.c
        ./Core/Src/tim.c
        ./Core/Src/usart.c
        ./Core/Src/stm32h7xx_it.c
        ./Core/Src/stm32h7xx_hal_msp.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pcd_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_usb.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_flash_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_hsem.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mdma.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2c_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_exti.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_fmc.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_nor.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sram.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_nand.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sdram.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2s.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_i2s_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_sdmmc.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_delayblock.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sd.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sd_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mmc.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_mmc_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spi.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_spi_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_tim_ex.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart.c
        C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1/Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_uart_ex.c
        ../Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c
        ./USB_DEVICE/Class/usbd_audio.c
        ./Core/Src/sysmem.c
        ./Core/Src/syscalls.c
        ./Core/Startup/startup_stm32h747xx_CM7.s
        app/profile_main.cpp
        app/main-usb-audio.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../profiles/hqzy_cm7_usb_audio.cpp
        app/uart_bridge.c
        app/port_impl.c
    )

    target_link_directories(${target_name} PRIVATE)
    target_link_libraries(${target_name} PRIVATE Charm-os)
endfunction()

charm_configure_usb_audio_target(stn32h747_hqzy_CM7_usb_audio)

function(add_player_profile_hint_target target_name profile_name)
    add_custom_target(${target_name}
        COMMAND ${CMAKE_COMMAND} -E echo "在独立构建目录中配置并构建 ${profile_name}:"
        COMMAND ${CMAKE_COMMAND} -E echo "cmake -S ${CMAKE_CURRENT_SOURCE_DIR} -B cmake-build-${target_name} -G Ninja -DPLAYER_PROFILE=${profile_name}"
        COMMAND ${CMAKE_COMMAND} -E echo "cmake --build cmake-build-${target_name} -j 22"
        COMMENT "IDE 场景目标：${target_name} -> ${profile_name}"
        VERBATIM
    )
endfunction()

add_player_profile_hint_target(player_hqzy_cm7_usb_audio USB_AUDIO)
add_player_profile_hint_target(player_hqzy_cm7_usb_self_msc USB_SELF_MSC)
add_player_profile_hint_target(player_hqzy_cm7_usb_storage USB_STORAGE)

# Validate that STM32CubeMX code is compatible with C standard
if(CMAKE_C_STANDARD LESS 11)
    message(ERROR "Generated code requires C11 or higher")
endif()

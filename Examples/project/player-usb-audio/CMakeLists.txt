cmake_minimum_required(VERSION 4.0)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/player_usb_audio_target.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/player_usb_audio_port_config.cmake")

set(PLAYER_USB_AUDIO_PORT "" CACHE STRING "Player USB Audio port to build")
set_property(CACHE PLAYER_USB_AUDIO_PORT PROPERTY STRINGS g431)

set(PLAYER_USB_AUDIO_USB_STACK "charm_cdc" CACHE STRING
    "USB bring-up path for Player USB Audio")
set_property(CACHE PLAYER_USB_AUDIO_USB_STACK PROPERTY STRINGS
    charm_cdc
    st_uac)

set(_player_usb_audio_default_cube_g4_root "$ENV{STM32CUBE_G4_ROOT}")
if(NOT _player_usb_audio_default_cube_g4_root AND
   EXISTS "C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2")
    set(_player_usb_audio_default_cube_g4_root
        "C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_G4_V1.6.2")
endif()

set(PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT
    "${_player_usb_audio_default_cube_g4_root}"
    CACHE PATH
    "Path to the STM32Cube FW G4 package root (contains Drivers/ and Middlewares/)")

if(NOT PLAYER_USB_AUDIO_PORT)
    message(FATAL_ERROR
        "PLAYER_USB_AUDIO_PORT is required. "
        "Use the 'g431-debug' preset or pass -DPLAYER_USB_AUDIO_PORT=g431 with the matching toolchain.")
endif()

if(NOT EXISTS "${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}/Drivers/STM32G4xx_HAL_Driver/Inc/stm32g4xx_hal.h")
    message(FATAL_ERROR
        "PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT is not valid: "
        "'${PLAYER_USB_AUDIO_STM32CUBE_G4_ROOT}'. "
        "Set it explicitly or export STM32CUBE_G4_ROOT before configuring.")
endif()

player_usb_audio_resolve_port("${PLAYER_USB_AUDIO_PORT}")

project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM CXX)
message("Build type: " ${CMAKE_BUILD_TYPE})
message("Player USB Audio port: " ${PLAYER_USB_AUDIO_PORT})

player_usb_audio_configure_target(
    TARGET ${CMAKE_PROJECT_NAME}
    LOCAL_ROOT "${PLAYER_USB_AUDIO_PORT_LOCAL_ROOT}"
    APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/app"
    STM32CUBEMX_DIR "${PLAYER_USB_AUDIO_PORT_STM32CUBEMX_DIR}"
    STM32_DEVICE "${PLAYER_USB_AUDIO_PORT_STM32_DEVICE}"
    CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
)

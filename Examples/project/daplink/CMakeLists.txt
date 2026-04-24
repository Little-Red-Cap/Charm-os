cmake_minimum_required(VERSION 4.0)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/daplink_target.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/daplink_port_config.cmake")

set(DAPLINK_PORT "" CACHE STRING "DAPLink port to build")
set_property(CACHE DAPLINK_PORT PROPERTY STRINGS f103 g431 h503)

if(NOT DAPLINK_PORT)
    message(FATAL_ERROR
        "DAPLINK_PORT is required. "
        "Use a root preset such as 'f103-debug', 'g431-debug', or 'h503-debug', "
        "or pass -DDAPLINK_PORT=<port> together with a matching CMAKE_TOOLCHAIN_FILE.")
endif()

daplink_resolve_port("${DAPLINK_PORT}")

project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM CXX)
message("Build type: " ${CMAKE_BUILD_TYPE})
message("DAPLink port: " ${DAPLINK_PORT})

daplink_configure_target(
    TARGET ${CMAKE_PROJECT_NAME}
    LOCAL_ROOT "${DAPLINK_PORT_LOCAL_ROOT}"
    APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/app"
    STM32CUBEMX_DIR "${DAPLINK_PORT_STM32CUBEMX_DIR}"
    STM32_DEVICE "${DAPLINK_PORT_STM32_DEVICE}"
    EXTRA_CPP_SOURCES ${DAPLINK_PORT_EXTRA_CPP_SOURCES}
    CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
)

cmake_minimum_required(VERSION 4.0)

include("${CMAKE_CURRENT_LIST_DIR}/../cmake/daplink_target.cmake")
include("cmake/gcc-arm-none-eabi.cmake")

project(${CMAKE_PROJECT_NAME} LANGUAGES C ASM CXX)
message("Build type: " ${CMAKE_BUILD_TYPE})

daplink_configure_target(
    TARGET ${CMAKE_PROJECT_NAME}
    LOCAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}"
    APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../app"
    STM32CUBEMX_DIR "cmake/stm32cubemx"
    STM32_DEVICE STM32F103xx
    CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.."
)

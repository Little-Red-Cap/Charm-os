cmake_minimum_required(VERSION 4.0)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/daplink_target.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/daplink_port_config.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/daplink_stm32cube.cmake")

set(DAPLINK_DEFAULT_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/g431" CACHE PATH
    "Fallback DAPLink port directory used when DAPLINK_PORT_DIR is not provided")
set(DAPLINK_PORT_DIR "" CACHE PATH "DAPLink port directory to build")
set(DAPLINK_STM32CUBE_F1_ROOT "" CACHE PATH "STM32Cube firmware package root for STM32F1 ports")
set(DAPLINK_STM32CUBE_G4_ROOT "" CACHE PATH "STM32Cube firmware package root for STM32G4 ports")
set(DAPLINK_STM32CUBE_H5_ROOT "" CACHE PATH "STM32Cube firmware package root for STM32H5 ports")
unset(DAPLINK_PORT CACHE)
daplink_resolve_port_dir("${DAPLINK_PORT_DIR}" DAPLINK_PORT_DIR_ABS DAPLINK_PORT_MANIFEST)
set(DAPLINK_PORT_DIR "${DAPLINK_PORT_DIR_ABS}" CACHE PATH "DAPLink port directory to build" FORCE)

include("${DAPLINK_PORT_MANIFEST}")

get_filename_component(_daplink_port_dir_name "${DAPLINK_PORT_DIR_ABS}" NAME)
if(NOT "${_daplink_port_dir_name}" STREQUAL "${DAPLINK_PORT_NAME}")
    message(FATAL_ERROR
        "DAPLINK_PORT_DIR must point to the matching port directory. "
        "Got '${DAPLINK_PORT_DIR_ABS}', but the manifest declares '${DAPLINK_PORT_NAME}'.")
endif()

if(NOT DEFINED DAPLINK_PORT_TOOLCHAIN_FILE OR "${DAPLINK_PORT_TOOLCHAIN_FILE}" STREQUAL "")
    message(FATAL_ERROR
        "Port manifest '${DAPLINK_PORT_MANIFEST}' must define DAPLINK_PORT_TOOLCHAIN_FILE.")
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE OR "${CMAKE_TOOLCHAIN_FILE}" STREQUAL "")
    set(CMAKE_TOOLCHAIN_FILE "${DAPLINK_PORT_TOOLCHAIN_FILE}" CACHE FILEPATH
        "Toolchain file selected by the DAPLink port" FORCE)
endif()

project(daplink LANGUAGES C ASM CXX)

if(DEFINED DAPLINK_TOOLCHAIN_PORT AND NOT "${DAPLINK_TOOLCHAIN_PORT}" STREQUAL "${DAPLINK_PORT_NAME}")
    message(FATAL_ERROR
        "Loaded toolchain port does not match the selected DAPLink port.\n"
        "Selected port: ${DAPLINK_PORT_NAME}\n"
        "Toolchain port: ${DAPLINK_TOOLCHAIN_PORT}\n"
        "Expected toolchain wrapper: ${DAPLINK_PORT_TOOLCHAIN_FILE}\n"
        "Current toolchain entry: ${CMAKE_TOOLCHAIN_FILE}")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "DAPLink port: ${DAPLINK_PORT_NAME}")
message(STATUS "DAPLink port dir: ${DAPLINK_PORT_DIR_ABS}")
message(STATUS "DAPLink binary dir: ${CMAKE_BINARY_DIR}")

foreach(required_var
    DAPLINK_PORT_NAME
    DAPLINK_PORT_LINKER_SCRIPT
    DAPLINK_PORT_TARGET_FLAGS
)
    if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
        message(FATAL_ERROR
            "Port manifest '${DAPLINK_PORT_MANIFEST}' must define ${required_var}.")
    endif()
endforeach()

daplink_configure_target(
    TARGET ${PROJECT_NAME}
    LOCAL_ROOT "${DAPLINK_PORT_DIR_ABS}"
    APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/app"
    PORT_NAME "${DAPLINK_PORT_NAME}"
    PORT_LINKER_SCRIPT "${DAPLINK_PORT_LINKER_SCRIPT}"
    PORT_TARGET_FLAGS ${DAPLINK_PORT_TARGET_FLAGS}
    EXTRA_CPP_SOURCES ${DAPLINK_PORT_EXTRA_CPP_SOURCES}
    CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
)

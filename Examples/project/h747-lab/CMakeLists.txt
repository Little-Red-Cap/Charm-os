cmake_minimum_required(VERSION 4.0)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
        "In-source CMake builds are disabled for h747-lab. "
        "Use CMakePresets.json so outputs stay under cmake-build-*.")
endif()

set(H747_LAB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(CHARM_ROOT "${H747_LAB_ROOT}/../../..")
set(BARE_WORKSPACE_ROOT "${CHARM_ROOT}/..")
set(H747_LAB_FALLBACK_BARE_WORKSPACE_ROOT "G:/Project/BareWorkspace"
    CACHE PATH "Fallback BareWorkspace root used when this project is built from a detached worktree")
set(DRAFT_ROOT "${CHARM_ROOT}/Draft" CACHE PATH "Charm CubeMX Draft root")
if((NOT EXISTS "${DRAFT_ROOT}/CM7") AND
   EXISTS "${H747_LAB_FALLBACK_BARE_WORKSPACE_ROOT}/Charm-os/Draft/CM7")
    set(DRAFT_ROOT "${H747_LAB_FALLBACK_BARE_WORKSPACE_ROOT}/Charm-os/Draft"
        CACHE PATH "Charm CubeMX Draft root" FORCE)
endif()
set(DRAFT_CM7_ROOT "${DRAFT_ROOT}/CM7")
set(STM32CUBE_H7_ROOT "C:/Users/Joho/STM32Cube/Repository/STM32Cube_FW_H7_V1.12.1"
    CACHE PATH "STM32Cube FW H7 package root")
set(HAL_ROOT "${STM32CUBE_H7_ROOT}/Drivers/STM32H7xx_HAL_Driver")
set(STM32_LINKER_SCRIPT "${BARE_WORKSPACE_ROOT}/scripts/STM32H747XIHX_FLASH.ld"
    CACHE FILEPATH "STM32H747 CM7 flash linker script")
if((NOT EXISTS "${STM32_LINKER_SCRIPT}") AND
   EXISTS "${H747_LAB_FALLBACK_BARE_WORKSPACE_ROOT}/scripts/STM32H747XIHX_FLASH.ld")
    set(STM32_LINKER_SCRIPT
        "${H747_LAB_FALLBACK_BARE_WORKSPACE_ROOT}/scripts/STM32H747XIHX_FLASH.ld"
        CACHE FILEPATH "STM32H747 CM7 flash linker script" FORCE)
endif()

if(NOT EXISTS "${DRAFT_CM7_ROOT}/Core/Inc")
    message(FATAL_ERROR
        "Cannot find Draft CM7 root: ${DRAFT_CM7_ROOT}. "
        "Set DRAFT_ROOT or H747_LAB_FALLBACK_BARE_WORKSPACE_ROOT.")
endif()
if(NOT EXISTS "${STM32_LINKER_SCRIPT}")
    message(FATAL_ERROR
        "Cannot find STM32H747 linker script: ${STM32_LINKER_SCRIPT}. "
        "Set STM32_LINKER_SCRIPT or H747_LAB_FALLBACK_BARE_WORKSPACE_ROOT.")
endif()

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE
        "${H747_LAB_ROOT}/cmake/toolchains/h747-lab-gcc-arm-none-eabi.cmake"
        CACHE FILEPATH "ARM GCC toolchain file for h747-lab")
endif()

project(h747_lab LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(H747_LAB_DISPLAY_PANEL_PROFILE "dts_2lane" CACHE STRING
    "HX8394D init table profile for h747_lab_display_demo")
set_property(CACHE H747_LAB_DISPLAY_PANEL_PROFILE PROPERTY STRINGS
    dts_2lane
    github4lane_2lane)

if((NOT H747_LAB_DISPLAY_PANEL_PROFILE STREQUAL "dts_2lane") AND
   (NOT H747_LAB_DISPLAY_PANEL_PROFILE STREQUAL "github4lane_2lane"))
    message(FATAL_ERROR
        "Unsupported H747_LAB_DISPLAY_PANEL_PROFILE='${H747_LAB_DISPLAY_PANEL_PROFILE}'. "
        "Expected one of: dts_2lane, github4lane_2lane.")
endif()

include("${H747_LAB_ROOT}/cmake/h747_lab_sources.cmake")
include("${H747_LAB_ROOT}/cmake/h747_lab_memory_evidence.cmake")
include("${H747_LAB_ROOT}/cmake/h747_lab_target.cmake")

h747_lab_add_profile(diag_shell)
h747_lab_add_profile(display_demo)
h747_lab_add_profile(display_raster_demo)
h747_lab_add_profile(input_probe)
h747_lab_add_profile(player)
h747_lab_add_profile(player_md3)
h747_lab_add_profile(storage_firmware_runtime)
h747_lab_add_profile(usb_msc_legacy_probe)
h747_lab_add_profile(posix_lab)
h747_lab_add_profile(app_lab)
h747_lab_add_profile(dev_loader)

message(STATUS "========================================")
message(STATUS " H747 Lab")
message(STATUS "========================================")
message(STATUS "Charm root: ${CHARM_ROOT}")
message(STATUS "Draft CM7 root: ${DRAFT_CM7_ROOT}")
message(STATUS "HAL root: ${HAL_ROOT}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Targets: h747_lab_diag_shell; h747_lab_display_demo; h747_lab_display_raster_demo; h747_lab_input_probe; h747_lab_player; h747_lab_player_md3; h747_lab_storage_firmware_runtime; h747_lab_usb_msc_legacy_probe; h747_lab_posix_lab; h747_lab_app_lab; h747_lab_dev_loader")
message(STATUS "Display panel profile: ${H747_LAB_DISPLAY_PANEL_PROFILE}")
message(STATUS "Linker script: ${STM32_LINKER_SCRIPT}")
message(STATUS "========================================")

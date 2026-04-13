cmake_minimum_required(VERSION 4.0)
project(charm-aarch64-qemu LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

get_filename_component(CHARM_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../.."
    ABSOLUTE)

set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_FATFS OFF CACHE BOOL "" FORCE)
set(CHARM_AUDIO_USE_VFS OFF CACHE BOOL "" FORCE)
set(CHARM_BUILD_PLAYER OFF CACHE BOOL "" FORCE)

add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
include("${CHARM_ROOT}/cmake/CharmTargetConfig.cmake")

enable_language(C ASM CXX)

add_library(charm_cfg_aarch64_qemu INTERFACE)
charm_add_config_interface(charm_cfg_aarch64_qemu
    ARCH aarch64
    PLATFORM qemu_virt
    BAREMETAL)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    startup.S
    main.cpp
    early_uart.cpp
    qemu_virt_platform.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=${CHARM_AARCH64_CPU})

target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=${CHARM_AARCH64_CPU}
    -Wl,--build-id=none
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

charm_apply_target_profile(${PROJECT_NAME}
    ARCH aarch64
    BAREMETAL)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Charm-os
    charm_cfg_aarch64_qemu)

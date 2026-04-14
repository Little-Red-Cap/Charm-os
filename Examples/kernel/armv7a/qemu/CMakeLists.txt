cmake_minimum_required(VERSION 4.0)
project(charm-armv7a-qemu LANGUAGES C CXX ASM)

option(CHARM_QEMU_SEMIHOST_DEBUG "Enable semihosting debug breadcrumbs for QEMU runs" OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if (DEFINED CHARM_TARGET_ARCH AND NOT CHARM_TARGET_ARCH STREQUAL "armv7a")
    message(FATAL_ERROR
        "This example only supports CHARM_TARGET_ARCH=armv7a, got: ${CHARM_TARGET_ARCH}")
endif()
if (DEFINED CHARM_TARGET_PLATFORM AND NOT CHARM_TARGET_PLATFORM STREQUAL "qemu_virt")
    message(FATAL_ERROR
        "This example only supports CHARM_TARGET_PLATFORM=qemu_virt, got: ${CHARM_TARGET_PLATFORM}")
endif()
if (DEFINED CHARM_BAREMETAL AND NOT CHARM_BAREMETAL)
    message(FATAL_ERROR
        "This example requires CHARM_BAREMETAL=ON")
endif()

get_filename_component(CHARM_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../.."
    ABSOLUTE)
include("${CHARM_ROOT}/cmake/CharmTargetConfig.cmake")

charm_add_config_interface(charm_cfg_armv7a_qemu
    ARCH armv7a
    PLATFORM qemu_virt
    BAREMETAL)

set(CHARM_CONFIG_TARGETS charm_cfg_armv7a_qemu)
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_FATFS OFF CACHE BOOL "" FORCE)
set(CHARM_AUDIO_USE_VFS OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
set(CHARM_BUILD_PLAYER OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
unset(CHARM_CONFIG_TARGETS)

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    vectors.S
    startup.S
    main.cpp
    armv7a_cpu.cpp
    armv7a_arch_timer.cpp
    early_uart.cpp
    qemu_virt_platform.cpp
    exceptions.cpp
    irq_timer.cpp
    runtime_stubs.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=${CHARM_ARMV7A_CPU}
    -marm)

target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=${CHARM_ARMV7A_CPU}
    -marm
    -Wl,--build-id=none
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

charm_apply_target_profile(${PROJECT_NAME}
    BAREMETAL)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Charm-os
    charm_cfg_armv7a_qemu)

if (CHARM_QEMU_SEMIHOST_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_QEMU_SEMIHOST_DEBUG=1)
endif()

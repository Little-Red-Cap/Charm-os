cmake_minimum_required(VERSION 4.0)
project(rtos-qemu-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
include("${CHARM_ROOT}/cmake/CharmTargetConfig.cmake")
charm_add_config_interface(charm_cfg_kernel_rtos_qemu
    COMPILE_DEFINITIONS CHARM_KERNEL_REQUIRE_SSU_META=1)
set(CHARM_CONFIG_TARGETS charm_cfg_kernel_rtos_qemu)
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
unset(CHARM_CONFIG_TARGETS)

add_executable(${target_name}
    startup.cpp
    main.cpp
)

target_link_libraries(${target_name} PRIVATE
    Charm-os
    charm_cfg_kernel_rtos_qemu)

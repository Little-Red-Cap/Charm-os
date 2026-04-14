cmake_minimum_required(VERSION 4.0)
project(os-example-win)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
#        main.cpp
        main_m1.cpp
#        main_m2.cpp
#        main_m3.cpp
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include("${CHARM_ROOT}/cmake/CharmTargetConfig.cmake")
charm_add_config_interface(charm_cfg_kernel_windows
    COMPILE_DEFINITIONS CHARM_KERNEL_REQUIRE_SSU_META=1)
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)
charm_apply_config_targets(Charm-build-config charm_cfg_kernel_windows)
charm_apply_config_targets(${target_name} charm_cfg_kernel_windows)

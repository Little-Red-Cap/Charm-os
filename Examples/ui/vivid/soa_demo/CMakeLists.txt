cmake_minimum_required(VERSION 4.0)
project(vivid-soa-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
option(CHARM_VIVID_SOA_TRACE_INPUT "Enable SoA trace-only hooks for regression" OFF)

add_executable(${target_name}
    main.cpp
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
include("${CHARM_ROOT}/Examples/cmake/ExampleTemplate.cmake")
include("${CHARM_ROOT}/cmake/CharmTargetConfig.cmake")

charm_example_sdl3_options()

set(CHARM_ENABLE_FREETYPE ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)
set(CHARM_VIVID_SCREEN_WIDTH 800 CACHE STRING "" FORCE)
set(CHARM_VIVID_SCREEN_HEIGHT 480 CACHE STRING "" FORCE)

if (CHARM_VIVID_SOA_TRACE_INPUT)
    charm_add_config_interface(charm_cfg_vivid_soa_demo
        COMPILE_DEFINITIONS VIVID_SOA_TRACE_INPUT=1)
    set(_vivid_soa_demo_configs charm_cfg_vivid_soa_demo)
endif()

charm_example_link_charm(${target_name} "${CHARM_ROOT}" ${_vivid_soa_demo_configs})

target_sources(${target_name} PRIVATE
)

charm_example_link_sdl3(${target_name} "${CHARM_ROOT}")



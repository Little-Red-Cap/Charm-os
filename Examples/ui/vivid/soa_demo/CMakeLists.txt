cmake_minimum_required(VERSION 4.0)
project(vivid-soa-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
option(CHARM_VIVID_SOA_TRACE_INPUT "Enable SoA trace-only hooks for regression" OFF)

enable_testing()

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
set(CHARM_VIVID_SCREEN_WIDTH 800 CACHE STRING "")
set(CHARM_VIVID_SCREEN_HEIGHT 480 CACHE STRING "")

if (CHARM_VIVID_SOA_TRACE_INPUT)
    charm_add_config_interface(charm_cfg_vivid_soa_demo
        COMPILE_DEFINITIONS VIVID_SOA_TRACE_INPUT=1)
    set(_vivid_soa_demo_configs charm_cfg_vivid_soa_demo)
endif()

charm_example_link_charm(${target_name} "${CHARM_ROOT}" ${_vivid_soa_demo_configs})

target_sources(${target_name} PRIVATE
)

charm_example_link_sdl3(${target_name} "${CHARM_ROOT}")

if (CHARM_VIVID_SOA_TRACE_INPUT)
    add_test(NAME ${target_name}
        COMMAND $<TARGET_FILE:${target_name}> --soa-ci --regress-ui)
    set_tests_properties(${target_name} PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        PASS_REGULAR_EXPRESSION "\\[soa-ci\\].*ok=1.*failed_cmds=0.*workspace_overflow=0.*table_tree_ok=1.*ui_ok=1"
        FAIL_REGULAR_EXPRESSION "\\[ERR\\]|result=fail|\\[soa-ci\\] ok=0")
endif()



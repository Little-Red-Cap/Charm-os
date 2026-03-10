cmake_minimum_required(VERSION 4.0)
project(vivid-soa-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
option(CHARM_VIVID_SOA_TRACE_INPUT "Enable SoA trace-only hooks for regression" OFF)

add_executable(${target_name}
    main.cpp
)

target_compile_definitions(${target_name} PRIVATE
    CHARM_VIVID_SCREEN_WIDTH=800
    CHARM_VIVID_SCREEN_HEIGHT=480
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
include("${CHARM_ROOT}/Examples/cmake/ExampleTemplate.cmake")

charm_example_sdl3_options()

set(CHARM_ENABLE_SDL3 ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)

charm_example_link_charm(${target_name} "${CHARM_ROOT}")

if (CHARM_VIVID_SOA_TRACE_INPUT)
    target_compile_definitions(${target_name} PRIVATE VIVID_SOA_TRACE_INPUT=1)
    target_compile_definitions(Charm-os PRIVATE VIVID_SOA_TRACE_INPUT=1)
endif()

target_sources(${target_name} PRIVATE
)

charm_example_link_sdl3(${target_name} "${CHARM_ROOT}")



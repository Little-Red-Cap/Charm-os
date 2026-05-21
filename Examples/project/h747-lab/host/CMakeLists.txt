cmake_minimum_required(VERSION 4.0)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source cmake-build-* directory for h747-lab host.")
endif()

project(h747_lab_host LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

set(H747_LAB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

add_executable(h747_lab_host_display_raster_demo
    "${H747_LAB_ROOT}/host/display_raster_demo_host.cpp")

target_include_directories(h747_lab_host_display_raster_demo PRIVATE
    "${H747_LAB_ROOT}")

target_compile_options(h747_lab_host_display_raster_demo PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

add_executable(h747_lab_host_player
    "${H747_LAB_ROOT}/host/player_host.cpp")

target_include_directories(h747_lab_host_player PRIVATE
    "${H747_LAB_ROOT}")

target_compile_options(h747_lab_host_player PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)

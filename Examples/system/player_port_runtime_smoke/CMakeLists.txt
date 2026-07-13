cmake_minimum_required(VERSION 4.0)
project(player-port-runtime-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include("${CHARM_ROOT}/Examples/cmake/ExampleTemplate.cmake")

set(CHARM_BUILD_FULL_RUNTIME OFF CACHE BOOL "" FORCE)
set(CHARM_BUILD_AUDIO_COMPONENT OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)

add_executable(${target_name} main.cpp)

target_sources(${target_name}
    PRIVATE
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS "${CHARM_ROOT}"
    FILES
        "${CHARM_ROOT}/Modules/io/hal/input.raw.cppm"
        "${CHARM_ROOT}/Modules/io/input/input.raw_event.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.raster.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.port.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.port_runtime.cppm"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME player_port_runtime_smoke COMMAND ${target_name})
set_tests_properties(player_port_runtime_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[player-port-runtime-smoke\\] ok")

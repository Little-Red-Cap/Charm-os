cmake_minimum_required(VERSION 4.0)
project(player-md3-runtime-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include("${CHARM_ROOT}/Examples/cmake/ExampleTemplate.cmake")
include("${CHARM_ROOT}/cmake/CharmTargetConfig.cmake")

set(CHARM_BUILD_FULL_RUNTIME ON CACHE BOOL "" FORCE)
set(CHARM_BUILD_AUDIO_COMPONENT OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_MEDIA ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_AUDIO_USE_VFS ON CACHE BOOL "" FORCE)
set(CHARM_AUDIO_SINK_I2S OFF CACHE BOOL "" FORCE)
set(CHARM_VIVID_SCREEN_WIDTH 568 CACHE STRING "" FORCE)
set(CHARM_VIVID_SCREEN_HEIGHT 1210 CACHE STRING "" FORCE)
set(CHARM_VIVID_LAYER_CACHE_SLOTS 2 CACHE STRING "" FORCE)
set(CHARM_VIVID_LAYER_CACHE_WIDTH 568 CACHE STRING "" FORCE)
set(CHARM_VIVID_LAYER_CACHE_HEIGHT 1210 CACHE STRING "" FORCE)

charm_add_config_interface(charm_cfg_player_md3_canonical
    COMPILE_DEFINITIONS
        CHARM_AUDIO_USE_VFS=1
        CHARM_AUDIO_SINK_PUMPED_NULL=1
        CHARM_PLAYER_STORAGE_DEFAULT=0
        CHARM_PLAYER_COVER_DECODE=0
        CHARM_PLAYER_FILE_FONTS=0
        CHARM_PLAYER_PLAYBACK_LOG=0
        CHARM_PLAYER_DEBUG_UI=0
        CHARM_PLAYER_LYRICS=1
        CHARM_PLAYER_COVER_THEME_EXTRACT=0
        CHARM_PLAYER_LAYERED_TRANSITIONS=0
        CHARM_PLAYER_PC_FONT_CACHE=0
        CHARM_PLAYER_PROFILE="canonical_smoke"
        CHARM_PLAYER_PRODUCT="PlayerMD3"
        CHARM_PLAYER_RESOURCE_FONT_PATH=""
        CHARM_PLAYER_RESOURCE_FONT_SMALL_PX=14
        CHARM_PLAYER_RESOURCE_FONT_NORMAL_PX=18
        CHARM_PLAYER_RESOURCE_FONT_LARGE_PX=76
        CHARM_PLAYER_STORAGE_IMAGE_PATH="")

add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
charm_apply_config_targets(Charm-build-config charm_cfg_player_md3_canonical)

include("${CHARM_ROOT}/Examples/project/player/cmake/player_md3_target.cmake")
player_add_md3_component(charm_player_md3
    CONFIG_TARGETS charm_cfg_player_md3_canonical)

add_executable(${target_name} main.cpp)
target_link_libraries(${target_name} PRIVATE charm_player_md3)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME player_md3_runtime_smoke COMMAND ${target_name})
set_tests_properties(player_md3_runtime_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[player-md3-runtime-smoke\\] ok")
add_test(NAME player_md3_runtime_rgb565_smoke COMMAND ${target_name} rgb565)
add_test(NAME player_md3_runtime_argb8888_smoke COMMAND ${target_name} argb8888)
set_tests_properties(
    player_md3_runtime_rgb565_smoke
    player_md3_runtime_argb8888_smoke
    PROPERTIES PASS_REGULAR_EXPRESSION "\\[player-md3-runtime-smoke\\] ok")

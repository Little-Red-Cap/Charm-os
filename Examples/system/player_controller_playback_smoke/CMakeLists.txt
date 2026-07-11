cmake_minimum_required(VERSION 4.0)
project(player-controller-playback-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include("${CHARM_ROOT}/Examples/cmake/ExampleTemplate.cmake")

set(CHARM_BUILD_FULL_RUNTIME ON CACHE BOOL "" FORCE)
set(CHARM_BUILD_AUDIO_COMPONENT OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_MEDIA ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_AUDIO_USE_VFS ON CACHE BOOL "" FORCE)
set(CHARM_AUDIO_SINK_I2S OFF CACHE BOOL "" FORCE)

include("${CHARM_ROOT}/cmake/CharmTargetConfig.cmake")
charm_add_config_interface(charm_cfg_player_controller_playback_smoke
    COMPILE_DEFINITIONS
        CHARM_AUDIO_USE_VFS=1
        CHARM_AUDIO_SINK_PUMPED_NULL=1)

add_executable(${target_name}
    main.cpp
)

target_sources(${target_name}
    PRIVATE
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS
        "${CHARM_ROOT}"
    FILES
        "${CHARM_ROOT}/Examples/project/player/app-common/player.fixed_string.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.mcu_policy.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.app_config.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.cover_resource.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.font_cache.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.font_resource.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.font_resource_apply.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.fs_utils.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.host_features.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.input.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.lyrics.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.media_library.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.media_scan.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.playback.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.playback_session.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.product_config.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.recent_history.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.scene_runtime.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.stats_history.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.storage.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.time_utils.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.track_probe.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.controller.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.cover.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.cover_theme.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.ui.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.ui_builder.cppm"
)

target_compile_definitions(${target_name} PRIVATE
    CHARM_PLAYER_HOST_UI=0
    CHARM_PLAYER_HOST_STORAGE=0
    CHARM_PLAYER_HOST_COVER_DECODE=0
    CHARM_PLAYER_HOST_FILE_FONTS=0
    CHARM_PLAYER_PLAYBACK_LOG=0
    CHARM_PLAYER_DEBUG_UI=0
    CHARM_PLAYER_LYRICS=1
    CHARM_PLAYER_COVER_THEME_EXTRACT=0
    CHARM_PLAYER_LAYERED_TRANSITIONS=0
    CHARM_PLAYER_PC_FONT_CACHE=0
    CHARM_PLAYER_HOST_PROFILE="controller_playback_smoke"
    CHARM_PLAYER_PRODUCT="Player"
    CHARM_PLAYER_PLATFORM="host"
    CHARM_PLAYER_BOARD="smoke"
    CHARM_PLAYER_RESOURCE_FONT_PATH="/font/gflex_variable.ttf"
    CHARM_PLAYER_RESOURCE_FONT_SMALL_PX=14
    CHARM_PLAYER_RESOURCE_FONT_NORMAL_PX=18
    CHARM_PLAYER_RESOURCE_FONT_LARGE_PX=76
    CHARM_PLAYER_HOST_STORAGE_VHD_PATH=""
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

charm_example_link_charm(${target_name} "${CHARM_ROOT}"
    CONFIG_TARGETS charm_cfg_player_controller_playback_smoke)

enable_testing()
add_test(NAME player_controller_playback_smoke COMMAND ${target_name})
set_tests_properties(player_controller_playback_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[player-controller-playback-smoke\\] ok")

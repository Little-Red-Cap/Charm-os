cmake_minimum_required(VERSION 4.0)
project(player-playback-engine-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
include("${CHARM_ROOT}/Examples/cmake/ExampleTemplate.cmake")

set(CHARM_BUILD_FULL_RUNTIME OFF CACHE BOOL "" FORCE)
set(CHARM_BUILD_AUDIO_COMPONENT ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_AUDIO_USE_VFS OFF CACHE BOOL "" FORCE)
set(CHARM_AUDIO_SINK_I2S OFF CACHE BOOL "" FORCE)

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
        "${CHARM_ROOT}/Examples/project/player/app-common/player.host_features.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.playback.cppm"
        "${CHARM_ROOT}/Examples/project/player/app-common/player.playback_session.cppm"
)

target_compile_definitions(${target_name} PRIVATE
    CHARM_PLAYER_HOST_UI=0
    CHARM_PLAYER_HOST_STORAGE=0
    CHARM_PLAYER_HOST_COVER_DECODE=0
    CHARM_PLAYER_HOST_FILE_FONTS=0
    CHARM_PLAYER_PLAYBACK_LOG=0
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

charm_example_link_charm(${target_name} "${CHARM_ROOT}" LINK_TARGET Charm-audio)

enable_testing()
add_test(NAME player_playback_engine_smoke COMMAND ${target_name})
set_tests_properties(player_playback_engine_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[player-playback-engine-smoke\\] ok")

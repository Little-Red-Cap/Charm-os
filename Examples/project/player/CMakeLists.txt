cmake_minimum_required(VERSION 4.0)
project(charm-player LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

option(CHARM_PLAYER_BUILD_LEGACY_VARIANTS
    "Build legacy Win Vivid/Ink preview targets instead of the canonical component"
    OFF)
option(CHARM_PLAYER_BUILD_CANONICAL_SMOKES
    "Build the platform-independent Player Port and MD3 runtime smokes"
    ON)
option(CHARM_PLAYER_BUILD_HOST_SDL3
    "Build the thin Player MD3 adapter over Charm Host SDL3"
    OFF)
option(CHARM_PLAYER_DEBUG_UI "Build Player-owned debug UI modules" OFF)

if (CHARM_PLAYER_BUILD_LEGACY_VARIANTS)
    add_subdirectory(win)
    return()
endif()

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
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
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/player_md3_vivid_product.cmake")
vivid_configure_product_target(
    TARGET Charm-ui
    PROFILE "${CHARM_PLAYER_VIVID_PROFILE}"
    SCREEN_WIDTH 568
    SCREEN_HEIGHT 1210
    PIXEL_FORMAT RGB888
    LAYER_CACHE_SLOTS 2
    LAYER_CACHE_WIDTH 568
    LAYER_CACHE_HEIGHT 1210
    RUNTIME_SCENE_INSTANCES 1
    STATIC_MEMORY_BUDGET_BYTES 6291456
    STATIC_MEMORY_MIN_HEADROOM_BYTES 524288
    MAX_HOT_STACK_FRAME_BYTES 4096)

charm_add_config_interface(charm_cfg_player_md3_runtime
    COMPILE_DEFINITIONS
        CHARM_AUDIO_SINK_PUMPED_NULL=1)

charm_add_config_interface(charm_cfg_player_md3_product
    COMPILE_DEFINITIONS
        CHARM_PLAYER_STORAGE_DEFAULT=0
        CHARM_PLAYER_COVER_DECODE=0
        CHARM_PLAYER_FILE_FONTS=0
        CHARM_PLAYER_PLAYBACK_LOG=0
        CHARM_PLAYER_DEBUG_UI=$<BOOL:${CHARM_PLAYER_DEBUG_UI}>
        CHARM_PLAYER_LYRICS=1
        CHARM_PLAYER_LYRICS_MAX_LINES=192
        CHARM_PLAYER_LYRICS_LINE_TEXT_CAPACITY=160
        CHARM_PLAYER_LYRICS_RAW_READ_BYTES=32768
        CHARM_PLAYER_REQUIRE_ICON_ARENA=0
        CHARM_PLAYER_RUNTIME_FILE_FONTS=0
        CHARM_PLAYER_EXACT_FILE_FONTS=1
        CHARM_PLAYER_FONT_RESOURCE_PROBE_VFS=0
        CHARM_PLAYER_MEMORY_PROFILE_SYMBOLS=0
        CHARM_PLAYER_LEGACY_TOUCH_INPUT=0
        CHARM_PLAYER_COVER_THEME_EXTRACT=0
        CHARM_PLAYER_LAYERED_TRANSITIONS=0
        CHARM_PLAYER_PC_FONT_CACHE=0
        CHARM_PLAYER_PROFILE="canonical"
        CHARM_PLAYER_PRODUCT="PlayerMD3"
        CHARM_PLAYER_RESOURCE_FONT_PATH=""
        CHARM_PLAYER_RESOURCE_FONT_SMALL_PX=14
        CHARM_PLAYER_RESOURCE_FONT_NORMAL_PX=18
        CHARM_PLAYER_RESOURCE_FONT_LARGE_PX=76
        CHARM_PLAYER_STORAGE_IMAGE_PATH="")

add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
charm_apply_config_targets(Charm-build-config charm_cfg_player_md3_runtime)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/player_md3_target.cmake")
player_add_md3_component(charm_player_md3
    CONFIG_TARGETS charm_cfg_player_md3_product)

if (CHARM_PLAYER_BUILD_CANONICAL_SMOKES)
    include(CTest)

    add_executable(player_port_runtime_smoke
        "${CHARM_ROOT}/Examples/system/player_port_runtime_smoke/main.cpp")
    target_link_libraries(player_port_runtime_smoke PRIVATE Charm::player-md3)
    set_target_properties(player_port_runtime_smoke PROPERTIES
        OUTPUT_NAME "player-port-runtime-smoke")

    add_executable(player_md3_runtime_smoke
        "${CHARM_ROOT}/Examples/system/player_md3_runtime_smoke/main.cpp")
    target_link_libraries(player_md3_runtime_smoke PRIVATE Charm::player-md3)
    set_target_properties(player_md3_runtime_smoke PROPERTIES
        OUTPUT_NAME "player-md3-runtime-smoke")

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(player_port_runtime_smoke PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(player_md3_runtime_smoke PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME player_port_runtime_smoke COMMAND player_port_runtime_smoke)
    set_tests_properties(player_port_runtime_smoke PROPERTIES
        PASS_REGULAR_EXPRESSION "\\[player-port-runtime-smoke\\] ok")
    add_test(NAME player_md3_runtime_smoke COMMAND player_md3_runtime_smoke)
    set_tests_properties(player_md3_runtime_smoke PROPERTIES
        PASS_REGULAR_EXPRESSION "\\[player-md3-runtime-smoke\\] ok")
    add_test(NAME player_md3_runtime_rgb565_smoke
        COMMAND player_md3_runtime_smoke rgb565)
    add_test(NAME player_md3_runtime_argb8888_smoke
        COMMAND player_md3_runtime_smoke argb8888)
    set_tests_properties(
        player_md3_runtime_rgb565_smoke
        player_md3_runtime_argb8888_smoke
        PROPERTIES PASS_REGULAR_EXPRESSION "\\[player-md3-runtime-smoke\\] ok")

    add_custom_target(player_canonical_smokes
        DEPENDS player_port_runtime_smoke player_md3_runtime_smoke)
endif()

if (CHARM_PLAYER_BUILD_HOST_SDL3)
    set(CHARM_HOST_SDL3_TESTING OFF CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
    set(SDL_VIDEO ON CACHE BOOL "" FORCE)
    set(SDL_RENDER ON CACHE BOOL "" FORCE)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL3_DISABLE_WINDOWS_MAIN ON CACHE BOOL "" FORCE)

    add_subdirectory("${CHARM_ROOT}/Backends/host/sdl3"
        "${CMAKE_BINARY_DIR}/charm-host-sdl3")

    add_executable(charm-player-md3-host
        "${CMAKE_CURRENT_SOURCE_DIR}/host/main.cpp")
    target_sources(charm-player-md3-host
        PRIVATE
        FILE_SET player_host_modules TYPE CXX_MODULES
        BASE_DIRS "${CHARM_ROOT}"
        FILES "${CMAKE_CURRENT_SOURCE_DIR}/host/player.host_sdl3_adapter.cppm")
    target_link_libraries(charm-player-md3-host
        PRIVATE Charm::player-md3 Charm::host-sdl3)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(charm-player-md3-host PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    include(CTest)
    add_test(NAME player_md3_host_sdl3_smoke
        COMMAND charm-player-md3-host --hidden --frames=3)
    set_tests_properties(player_md3_host_sdl3_smoke PROPERTIES
        PASS_REGULAR_EXPRESSION "player-md3-host: ok frames=3 presented=3")
endif()

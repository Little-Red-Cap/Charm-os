cmake_minimum_required(VERSION 4.0)
project(charm-player-win)
set(CMAKE_CXX_STANDARD 26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/product_player_identity.cmake")

message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_PRODUCT: ${PLAYER_PRODUCT}")
message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_PLATFORM: ${PLAYER_PLATFORM}")
message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_BOARD: ${PLAYER_BOARD}")
message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_SCENARIO: ${PLAYER_SCENARIO}")

option(CHARM_PLAYER_DEBUG_UI "Enable player debug UI" ON)
option(CHARM_PLAYER_SOA_TRACE "Enable SoA trace-only hooks for player" OFF)
option(CHARM_PLAYER_COVER_DEBUG "Enable cover debug log" OFF)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/product_player_host_scenarios.cmake")

message(STATUS "${CMAKE_PROJECT_NAME} PLAYER_HOST_SCENARIO_TARGET: ${PLAYER_HOST_SCENARIO_TARGET}")

add_executable(${PLAYER_TARGET_VIVID}
    ${PLAYER_MAIN_VIVID}
)
add_executable(${PLAYER_TARGET_INK}
    ${PLAYER_MAIN_INK}
)
add_executable(${PLAYER_TARGET_VIVID_MD3}
    ${PLAYER_MAIN_VIVID}
)

foreach(_target IN ITEMS ${PLAYER_TARGET_VIVID} ${PLAYER_TARGET_INK} ${PLAYER_TARGET_VIVID_MD3})
    target_compile_definitions(${_target} PRIVATE
        OUT_ENABLE_FLOAT
        CHARM_AUDIO_USE_VFS=1
        CHARM_PLAYER_PC_FONT_CACHE=1
        CHARM_PLAYER_PRODUCT="${PLAYER_PRODUCT}"
        CHARM_PLAYER_PLATFORM="${PLAYER_PLATFORM}"
        CHARM_PLAYER_BOARD="${PLAYER_BOARD}"
    )
    if (CHARM_PLAYER_DEBUG_UI)
        target_compile_definitions(${_target} PRIVATE CHARM_PLAYER_DEBUG_UI=1)
    else()
        target_compile_definitions(${_target} PRIVATE CHARM_PLAYER_DEBUG_UI=0)
    endif()
    if (CHARM_PLAYER_COVER_DEBUG)
        target_compile_definitions(${_target} PRIVATE CHARM_PLAYER_COVER_DEBUG=1)
    else()
        target_compile_definitions(${_target} PRIVATE CHARM_PLAYER_COVER_DEBUG=0)
    endif()
endforeach()

target_compile_definitions(${PLAYER_TARGET_INK} PRIVATE
    CHARM_PLAYER_SCENARIO_UI_PREVIEW_INK=1
)


set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
include("${CHARM_ROOT}/Examples/cmake/ExampleTemplate.cmake")

charm_example_sdl3_options()
set(SDL_AUDIO ON)

set(CHARM_ENABLE_SDL3 ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_MEDIA ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_FREETYPE ON CACHE BOOL "" FORCE)
set(CHARM_AUDIO_USE_VFS ON CACHE BOOL "" FORCE)
set(CHARM_VIVID_SCREEN_WIDTH 480 CACHE STRING "" FORCE)
set(CHARM_VIVID_SCREEN_HEIGHT 960 CACHE STRING "" FORCE)
set(CHARM_AUDIO_SINK_I2S OFF CACHE BOOL "" FORCE)
if (DEFINED CHARM_FATFS_ROOT)
    set(_fatfs_root "${CHARM_FATFS_ROOT}")
else()
    set(_fatfs_root "${CHARM_ROOT}/Modules/thirdparty/fatfs")
endif()
set(CHARM_FATFS_ROOT "${_fatfs_root}" CACHE PATH "" FORCE)
set(CHARM_ENABLE_FATFS OFF CACHE BOOL "" FORCE)
if (EXISTS "${_fatfs_root}/ff.c" AND EXISTS "${_fatfs_root}/ff.h")
    set(CHARM_ENABLE_FATFS ON CACHE BOOL "" FORCE)
elseif (EXISTS "${_fatfs_root}/source/ff.c" AND EXISTS "${_fatfs_root}/source/ff.h")
    set(CHARM_ENABLE_FATFS ON CACHE BOOL "" FORCE)
elseif (EXISTS "${_fatfs_root}/src/ff.c" AND EXISTS "${_fatfs_root}/src/ff.h")
    set(CHARM_ENABLE_FATFS ON CACHE BOOL "" FORCE)
endif()

set(CHARM_PLAYER_EXCLUDE_REGEX_BASE "/stm32f407/|/stn32h747[^/]*/|/stn32common/")
set(CHARM_PLAYER_EXCLUDE_REGEX_VIVID "${CHARM_PLAYER_EXCLUDE_REGEX_BASE}|/app-ink/")
set(CHARM_PLAYER_EXCLUDE_REGEX_INK "${CHARM_PLAYER_EXCLUDE_REGEX_BASE}|/app-vivid/")
set(CHARM_PLAYER_EXCLUDE_REGEX_MD3 "${CHARM_PLAYER_EXCLUDE_REGEX_BASE}|/app-ink/|/app-vivid/")

charm_example_sources_filtered(${PLAYER_TARGET_VIVID}
    "${CHARM_ROOT}"
    "${CHARM_PLAYER_EXCLUDE_REGEX_VIVID}"
    "${CHARM_ROOT}/Examples/project/player/app-common/*.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-vivid/*.cppm"
)
charm_example_sources(${PLAYER_TARGET_VIVID} "${CHARM_ROOT}"
    "${CHARM_ROOT}/Examples/ui/vivid/*.cppm"
    "${CHARM_ROOT}/Examples/common/pc/*.cppm"
)

charm_example_sources_filtered(${PLAYER_TARGET_INK}
    "${CHARM_ROOT}"
    "${CHARM_PLAYER_EXCLUDE_REGEX_INK}"
    "${CHARM_ROOT}/Examples/project/player/app-common/*.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-ink/**/*.cppm"
)
charm_example_sources_filtered(${PLAYER_TARGET_VIVID_MD3}
    "${CHARM_ROOT}"
    "${CHARM_PLAYER_EXCLUDE_REGEX_MD3}"
    "${CHARM_ROOT}/Examples/project/player/app-common/*.cppm"
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/*.cppm"
)
target_sources(${PLAYER_TARGET_VIVID_MD3} PRIVATE
    "${CHARM_ROOT}/Examples/project/player/app-vivid-MaterialDesign3/player.cover_theme.cppm"
)
charm_example_sources(${PLAYER_TARGET_INK} "${CHARM_ROOT}"
    "${CHARM_ROOT}/Examples/common/sdl3/backend.sdl3.cppm"
    "${CHARM_ROOT}/Examples/common/sdl3/sdl3.input.cppm"
    "${CHARM_ROOT}/Examples/common/pc/*.cppm"
)

charm_example_link_charm(${PLAYER_TARGET_VIVID} "${CHARM_ROOT}")
target_link_libraries(${PLAYER_TARGET_INK} PRIVATE Charm-os)
target_link_libraries(${PLAYER_TARGET_VIVID_MD3} PRIVATE Charm-os)

if (CHARM_PLAYER_SOA_TRACE)
    target_compile_definitions(${PLAYER_TARGET_VIVID} PRIVATE VIVID_SOA_TRACE_INPUT=1)
    target_compile_definitions(${PLAYER_TARGET_VIVID_MD3} PRIVATE VIVID_SOA_TRACE_INPUT=1)
    target_compile_definitions(Charm-os PRIVATE VIVID_SOA_TRACE_INPUT=1)
endif()

target_sources(${PLAYER_TARGET_VIVID} PRIVATE
)

set(CHARM_DR_LIBS_DIR "${CHARM_ROOT}/Modules/thirdparty/dr_libs")
if (EXISTS "${CHARM_DR_LIBS_DIR}/dr_mp3.h" AND EXISTS "${CHARM_DR_LIBS_DIR}/dr_flac.h")
    include("${CHARM_ROOT}/cmake/DRLibs.cmake")
    charm_link_dr_libs(${PLAYER_TARGET_VIVID})
    charm_link_dr_libs(${PLAYER_TARGET_INK})
    charm_link_dr_libs(${PLAYER_TARGET_VIVID_MD3})
endif()

charm_example_link_sdl3(${PLAYER_TARGET_VIVID} "${CHARM_ROOT}")
charm_example_link_sdl3(${PLAYER_TARGET_INK} "${CHARM_ROOT}")
charm_example_link_sdl3(${PLAYER_TARGET_VIVID_MD3} "${CHARM_ROOT}")

cmake_minimum_required(VERSION 4.0)
project(charm-player-win)

set(target_name ${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 26)
option(CHARM_PLAYER_DEBUG_UI "Enable player debug UI" ON)
option(CHARM_VIVID_SOA_ONLY "Enable SoA-only player entry" OFF)
option(CHARM_PLAYER_SOA_TRACE "Enable SoA trace-only hooks for player" OFF)

if (CHARM_VIVID_SOA_ONLY)
    set(PLAYER_MAIN main_soa.cpp)
else()
    set(PLAYER_MAIN main.cpp)
endif()

add_executable(${target_name}
    ${PLAYER_MAIN}
)

target_compile_definitions(${target_name} PRIVATE
    OUT_ENABLE_FLOAT
    CHARM_AUDIO_USE_VFS=1
    CHARM_VIVID_SCREEN_WIDTH=480
    CHARM_VIVID_SCREEN_HEIGHT=960
)
if (CHARM_PLAYER_DEBUG_UI)
    target_compile_definitions(${target_name} PRIVATE CHARM_PLAYER_DEBUG_UI=1)
else()
    target_compile_definitions(${target_name} PRIVATE CHARM_PLAYER_DEBUG_UI=0)
endif()

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
include("${CHARM_ROOT}/Examples/cmake/ExampleTemplate.cmake")

charm_example_sdl3_options()
set(SDL_AUDIO ON)

set(CHARM_ENABLE_SDL3 ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_MEDIA ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)
set(CHARM_VIVID_PROFILE "full" CACHE STRING "" FORCE)
set(CHARM_AUDIO_USE_VFS ON CACHE BOOL "" FORCE)
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

set(CHARM_PLAYER_EXCLUDE_REGEX "/stm32f407/|/stn32h747[^/]*/|/stn32common/")
if (CHARM_VIVID_SOA_ONLY)
    set(CHARM_PLAYER_EXCLUDE_REGEX "${CHARM_PLAYER_EXCLUDE_REGEX}|player\\.ui|player\\.controller")
endif()
charm_example_sources_filtered(${target_name}
    "${CHARM_ROOT}"
    "${CHARM_PLAYER_EXCLUDE_REGEX}"
    "${CHARM_ROOT}/Examples/project/player/*.cppm"
)
charm_example_sources(${target_name} "${CHARM_ROOT}"
    "${CHARM_ROOT}/Examples/ui/vivid/assets/*.cppm"
)
charm_example_link_charm(${target_name} "${CHARM_ROOT}")

if (CHARM_PLAYER_SOA_TRACE)
    target_compile_definitions(${target_name} PRIVATE VIVID_SOA_TRACE_INPUT=1)
    target_compile_definitions(Charm-os PRIVATE VIVID_SOA_TRACE_INPUT=1)
endif()

target_sources(${target_name} PRIVATE
)

set(CHARM_DR_LIBS_DIR "${CHARM_ROOT}/Modules/thirdparty/dr_libs")
if (EXISTS "${CHARM_DR_LIBS_DIR}/dr_mp3.h" AND EXISTS "${CHARM_DR_LIBS_DIR}/dr_flac.h")
    include("${CHARM_ROOT}/cmake/DRLibs.cmake")
    charm_link_dr_libs(${target_name})
endif()

charm_example_link_sdl3(${target_name} "${CHARM_ROOT}")



cmake_minimum_required(VERSION 4.0)
project(charm-player-win)

set(target_name ${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 26)
option(CHARM_PLAYER_DEBUG_UI "Enable player debug UI" ON)

add_executable(${target_name}
    main.cpp
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
set(CHARM_AUDIO_USE_VFS ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_FATFS ON CACHE BOOL "" FORCE)
set(CHARM_FETCHCONTENT_FATFS ON CACHE BOOL "" FORCE)
set(CHARM_FATFS_GIT_REPOSITORY "https://github.com/abbrev/fatfs.git" CACHE STRING "" FORCE)
set(CHARM_FATFS_GIT_TAG "master" CACHE STRING "" FORCE)

charm_example_sources(${target_name} "${CHARM_ROOT}"
    "${CHARM_ROOT}/Examples/project/player/*.cppm"
)
charm_example_link_charm(${target_name} "${CHARM_ROOT}")

target_sources(${target_name} PRIVATE
    "${CHARM_ROOT}/Examples/kernel/windows/port.kernel.windows.cpp"
    "${CHARM_ROOT}/Modules/io/out/out.port.template.cpp"
)

set(CHARM_DR_LIBS_DIR "${CHARM_ROOT}/Modules/thirdparty/dr_libs")
if (EXISTS "${CHARM_DR_LIBS_DIR}/dr_mp3.h" AND EXISTS "${CHARM_DR_LIBS_DIR}/dr_flac.h")
    include("${CHARM_ROOT}/cmake/DRLibs.cmake")
    charm_link_dr_libs(${target_name})
endif()

charm_example_link_sdl3(${target_name} "${CHARM_ROOT}")

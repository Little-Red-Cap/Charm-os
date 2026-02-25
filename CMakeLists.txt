cmake_minimum_required(VERSION 4.0)
project(Charm-os)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
option(CHARM_ENABLE_SDL3 "Enable SDL3-backed modules" OFF)
option(CHARM_ENABLE_MEDIA "Enable media modules" ON)
option(CHARM_ENABLE_UI_INK "Enable Ink UI modules" ON)
option(CHARM_ENABLE_UI_VIVID "Enable Vivid UI modules" ON)
option(CHARM_USE_ETL "Use ETL for core util aliases" ON)
option(CHARM_AUDIO_USE_VFS "Use VFS-backed audio file source" OFF)
option(CHARM_USE_ETL_EXPECTED "Use ETL expected instead of std::expected" OFF)
option(CHARM_ENABLE_FATFS "Enable FatFs-backed mount" OFF)
option(CHARM_BUILD_PLAYER "Build player example" OFF)
option(CHARM_ENABLE_DEPENDENCY_WHITELIST "Enforce dependency whitelist at configure time" ON)

add_library(${target_name})

#add_compile_options(
target_compile_options(${target_name} PRIVATE
        -Wall
#        -fno-exceptions
#        -fno-rtti
        #        -ffreestanding
        #        -nostdlib
)

set(CHARM_DR_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Modules/thirdparty/dr_libs")
if (EXISTS "${CHARM_DR_LIBS_DIR}/dr_mp3.h" AND EXISTS "${CHARM_DR_LIBS_DIR}/dr_flac.h")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DRLibs.cmake")
    charm_link_dr_libs(${target_name})
endif()

if (CHARM_USE_ETL)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/ETL.cmake")
    charm_link_etl(${target_name})
    if (CHARM_USE_ETL_EXPECTED)
        target_compile_definitions(${target_name} PRIVATE CHARM_USE_ETL_EXPECTED=1)
    endif()
endif()

if (CHARM_ENABLE_FATFS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FatFs.cmake")
    charm_link_fatfs(${target_name})
endif()

if (CHARM_AUDIO_USE_VFS)
    target_compile_definitions(${target_name} PRIVATE CHARM_AUDIO_USE_VFS=1)
endif()
if (CHARM_ENABLE_SDL3)
    target_compile_definitions(${target_name} PRIVATE CHARM_ENABLE_SDL3=1)
endif()

if (CHARM_ENABLE_DEPENDENCY_WHITELIST)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DependencyWhitelist.cmake")
    charm_enforce_dependency_whitelist()
endif()

target_compile_definitions(${target_name} PRIVATE
    CHARM_ENABLE_MEDIA=$<BOOL:${CHARM_ENABLE_MEDIA}>
    CHARM_ENABLE_UI_INK=$<BOOL:${CHARM_ENABLE_UI_INK}>
    CHARM_ENABLE_UI_VIVID=$<BOOL:${CHARM_ENABLE_UI_VIVID}>
)

file(GLOB_RECURSE MODULE_INTERFACE_UNITS CONFIGURE_DEPENDS "Modules/*.cppm")
if (NOT CHARM_ENABLE_SDL3)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/media/audio/audio_sink_sdl3.cppm"
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/media/audio/audio_player.cppm"
    )
endif()
if (NOT CHARM_ENABLE_MEDIA)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/media/")
endif()
if (NOT CHARM_ENABLE_UI_INK)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/ui/ink/")
endif()
if (NOT CHARM_ENABLE_UI_VIVID)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/ui/vivid/")
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/core/alg/alg_text_layout.cppm")
endif()
if (NOT CHARM_ENABLE_MEDIA OR NOT CHARM_ENABLE_UI_INK OR NOT CHARM_ENABLE_UI_VIVID)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/charm.domain.cppm")
endif()
target_sources(${target_name}
        PUBLIC
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Modules"
        FILES ${MODULE_INTERFACE_UNITS})

message(STATUS "${target_name} Examples: Ink/Windows -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/ink/windows")
message(STATUS "${target_name} Examples: Ink/STM32   -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/ink/stm32f103c8")
message(STATUS "${target_name} Draft Examples: Windows -> ${CMAKE_CURRENT_SOURCE_DIR}/Draft/Examples/windows")
message(STATUS "${target_name} Draft Examples: STM32   -> ${CMAKE_CURRENT_SOURCE_DIR}/Draft/Examples/stm32f103c8")
message(STATUS "${target_name} Examples: Kernel/Windows -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/kernel/windows")
message(STATUS "${target_name} Examples: Boot/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/boot/bootloader_demo")
message(STATUS "${target_name} Examples: Power/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/system/power_demo")
message(STATUS "${target_name} Examples: Audio/SDL3 -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/audio/sdl3_wav_demo")
message(STATUS "${target_name} Examples: FS/VSF -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/fs")
message(STATUS "${target_name} Examples: Shell/VSF -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/shell/vsf_service_shell")
message(STATUS "${target_name} Examples: Shell/FS+Module -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/shell/vsf_shell_fs_module")
message(STATUS "${target_name} Examples: Service/VSF -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/service/vsf_service_core")
message(STATUS "${target_name} Examples: Service/DS -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/service/service_ds_demo")
message(STATUS "${target_name} Examples: HAL/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/hal/hal_demo")
message(STATUS "${target_name} Examples: Alg/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/alg/alg_demo")

if (CHARM_BUILD_PLAYER)
    add_subdirectory(Examples/project/player)
endif()

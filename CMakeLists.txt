cmake_minimum_required(VERSION 4.0)
project(Charm-os)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
option(CHARM_ENABLE_SDL3 "Enable SDL3-backed modules" OFF)
option(CHARM_AUDIO_SINK_I2S "Enable I2S audio sink" OFF)
option(CHARM_ENABLE_MEDIA "Enable media modules" ON)
option(CHARM_ENABLE_UI_INK "Enable Ink UI modules" ON)
option(CHARM_ENABLE_UI_VIVID "Enable Vivid UI modules" ON)
set(CHARM_VIVID_FEATURESET "FULL" CACHE STRING "Vivid featureset: FULL or MCU_MIN")
set_property(CACHE CHARM_VIVID_FEATURESET PROPERTY STRINGS FULL MCU_MIN)
option(CHARM_AUDIO_USE_VFS "Use VFS-backed audio file source" OFF)
option(CHARM_ENABLE_FATFS "Enable FatFs-backed mount" OFF)
option(CHARM_BUILD_PLAYER "Build player example" OFF)
option(CHARM_ENABLE_DEPENDENCY_WHITELIST "Enforce dependency whitelist at configure time" OFF)
option(CHARM_ENABLE_BANNED_API_CHECK "Ban printf-family APIs in project code" OFF)
option(CHARM_BANNED_API_CHECK_EXAMPLES "Also check Examples/ for banned APIs" OFF)
option(CHARM_ENABLE_BANNED_HEADER_CHECK "Ban selected headers in project code" OFF)
option(CHARM_BANNED_HEADER_CHECK_EXAMPLES "Also check Examples/ for banned headers" OFF)
set(CHARM_BANNED_HEADERS "iostream;fstream;filesystem;regex;format;locale;codecvt" CACHE STRING "Semicolon list of banned headers")
set(CHARM_BANNED_HEADERS_EXCLUDE_REGEX "" CACHE STRING "Semicolon list of regexes to exclude from banned header scan")
option(CHARM_FORBID_SHARED_LIBS "Forbid shared/module library targets" OFF)
option(CHARM_ENABLE_ARCH_LINT_GUARD "Run arch_lint guard during build" OFF)

add_library(${target_name})

#add_compile_options(
target_compile_options(${target_name} PRIVATE
        -Wall
#        -fno-exceptions
#        -fno-rtti
        #        -ffreestanding
        #        -nostdlib
)

# dr_mp3 must avoid unaligned word loads on Cortex-M7 (stream data is byte-aligned).
if (NOT WIN32)
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/media/audio/audio_decoder_mp3.cppm"
        PROPERTIES COMPILE_OPTIONS "-mno-unaligned-access"
    )
endif()

set(CHARM_DR_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Modules/thirdparty/dr_libs")
if (EXISTS "${CHARM_DR_LIBS_DIR}/dr_mp3.h" AND EXISTS "${CHARM_DR_LIBS_DIR}/dr_flac.h")
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DRLibs.cmake")
    charm_link_dr_libs(${target_name})
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
if (CHARM_ENABLE_BANNED_API_CHECK)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BannedApiCheck.cmake")
    charm_enforce_banned_api_check()
endif()
if (CHARM_ENABLE_BANNED_HEADER_CHECK)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/BannedHeaderCheck.cmake")
    charm_enforce_banned_headers()
endif()
if (CHARM_FORBID_SHARED_LIBS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SharedLibCheck.cmake")
    charm_enforce_no_shared_libs()
endif()

target_compile_definitions(${target_name} PRIVATE
    CHARM_ENABLE_MEDIA=$<BOOL:${CHARM_ENABLE_MEDIA}>
    CHARM_ENABLE_UI_INK=$<BOOL:${CHARM_ENABLE_UI_INK}>
    CHARM_ENABLE_UI_VIVID=$<BOOL:${CHARM_ENABLE_UI_VIVID}>
)
set(CHARM_VIVID_KERNEL "SOA" CACHE STRING "Vivid kernel (fixed to SOA)")
set_property(CACHE CHARM_VIVID_KERNEL PROPERTY STRINGS SOA)
if (DEFINED CHARM_VIVID_SOA_ONLY)
    message(WARNING "CHARM_VIVID_SOA_ONLY 已废弃，Vivid 仅支持 SoA 内核")
endif()
if (DEFINED CHARM_VIVID_PROFILE)
    message(WARNING "CHARM_VIVID_PROFILE 已废弃，Vivid 仅支持 SoA 内核")
endif()

file(GLOB_RECURSE MODULE_INTERFACE_UNITS CONFIGURE_DEPENDS "Modules/*.cppm")
if (NOT CHARM_ENABLE_SDL3)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/media/audio/audio_sink_sdl3.cppm"
    )
endif()
if (NOT CHARM_AUDIO_SINK_I2S)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/media/audio/audio_sink_i2s.cppm"
    )
else()
    set(CHARM_PLAYER_I2S_SINK_MODULE
        "${CMAKE_CURRENT_SOURCE_DIR}/Examples/project/player/stn32common/audio_sink_i2s.cppm")
    if (EXISTS "${CHARM_PLAYER_I2S_SINK_MODULE}")
        list(APPEND MODULE_INTERFACE_UNITS "${CHARM_PLAYER_I2S_SINK_MODULE}")
    else()
        message(FATAL_ERROR
            "CHARM_AUDIO_SINK_I2S=ON but module is missing: ${CHARM_PLAYER_I2S_SINK_MODULE}")
    endif()
endif()
if (NOT CHARM_ENABLE_MEDIA)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/media/")
endif()
if (NOT CHARM_ENABLE_UI_INK)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/ui/ink/")
endif()
if (NOT CHARM_ENABLE_UI_VIVID)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/ui/vivid/")
endif()
if (NOT CHARM_ENABLE_UI_INK AND NOT CHARM_ENABLE_UI_VIVID)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/ui/common/")
endif()
if (NOT CHARM_ENABLE_MEDIA OR NOT CHARM_ENABLE_UI_INK OR NOT CHARM_ENABLE_UI_VIVID)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/charm.domain.cppm")
endif()
set(MODULE_BASE_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules"
    "${CMAKE_CURRENT_SOURCE_DIR}/Examples/project/player/stn32common")
if (CHARM_ENABLE_UI_VIVID)
    include("${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/vivid.cmake")
    vivid_collect_modules(${target_name} MODULE_INTERFACE_UNITS MODULE_BASE_DIRS)
endif()
target_sources(${target_name}
        PUBLIC
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS ${MODULE_BASE_DIRS}
        FILES ${MODULE_INTERFACE_UNITS})

message(STATUS "${target_name} Examples: Ink/Windows -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/ink/windows")
message(STATUS "${target_name} Examples: Ink/STM32   -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/ink/stm32f103c8")
message(STATUS "${target_name} Draft Examples: Windows -> ${CMAKE_CURRENT_SOURCE_DIR}/Draft/Examples/windows")
message(STATUS "${target_name} Draft Examples: STM32   -> ${CMAKE_CURRENT_SOURCE_DIR}/Draft/Examples/stm32f103c8")
message(STATUS "${target_name} Examples: Kernel/Windows -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/kernel/windows")
message(STATUS "${target_name} Examples: Boot/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/boot/bootloader_demo")
message(STATUS "${target_name} Examples: Power/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/system/power_demo")
message(STATUS "${target_name} Examples: Audio/SDL3 -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/audio/sdl3_wav_demo")
message(STATUS "${target_name} Examples: FS -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/fs")
message(STATUS "${target_name} Examples: Shell -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/shell/service_shell")
message(STATUS "${target_name} Examples: Service -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/service/service_core")
message(STATUS "${target_name} Examples: Service/DS -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/service/service_ds_demo")
message(STATUS "${target_name} Examples: HAL/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/hal/hal_demo")
message(STATUS "${target_name} Examples: Alg/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/alg/alg_demo")

set(ARCH_LINT_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/arch_lint.ps1")
find_program(ARCH_LINT_SHELL NAMES pwsh powershell)
if (ARCH_LINT_SHELL AND EXISTS "${ARCH_LINT_SCRIPT}")
    if (WIN32)
        set(ARCH_LINT_ARGS -NoProfile -ExecutionPolicy Bypass -File "${ARCH_LINT_SCRIPT}")
    else()
        set(ARCH_LINT_ARGS -NoProfile -File "${ARCH_LINT_SCRIPT}")
    endif()
    add_custom_target(arch_lint
        COMMAND "${ARCH_LINT_SHELL}" ${ARCH_LINT_ARGS}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    if (CHARM_ENABLE_ARCH_LINT_GUARD)
        add_dependencies(${target_name} arch_lint)
    endif()
endif()

if (CHARM_BUILD_PLAYER)
    add_subdirectory(Examples/project/player)
endif()

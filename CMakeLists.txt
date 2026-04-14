cmake_minimum_required(VERSION 4.0)
project(Charm-os)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
option(CHARM_ENABLE_SDL3 "Enable SDL3-backed modules" OFF)
option(CHARM_AUDIO_SINK_I2S "Enable I2S audio sink" OFF)
option(CHARM_ENABLE_MEDIA "Enable media modules" ON)
option(CHARM_ENABLE_UI_INK "Enable Ink UI modules" ON)
option(CHARM_ENABLE_UI_VIVID "Enable Vivid UI modules" ON)
option(CHARM_ENABLE_POSIX "Enable POSIX modules" ON)
set(CHARM_VIVID_FEATURESET "FULL" CACHE STRING "Vivid featureset: FULL or MCU_MIN")
set_property(CACHE CHARM_VIVID_FEATURESET PROPERTY STRINGS FULL MCU_MIN)
option(CHARM_ENABLE_FREETYPE "Enable FreeType font loader" OFF)
option(CHARM_AUDIO_USE_VFS "Use VFS-backed audio file source" OFF)
option(CHARM_ENABLE_FATFS "Enable FatFs-backed mount" OFF)
option(CHARM_BUILD_PLAYER "Build player example" OFF)
option(CHARM_ENABLE_DEPENDENCY_WHITELIST "Enforce dependency whitelist at configure time" OFF)
option(CHARM_ENABLE_BANNED_API_CHECK "Ban printf-family APIs in project code" OFF)
option(CHARM_BANNED_API_CHECK_EXAMPLES "Also check Examples/ for banned APIs" OFF)
option(CHARM_ENABLE_BANNED_HEADER_CHECK "Ban selected headers in project code" OFF)
option(CHARM_BANNED_HEADER_CHECK_EXAMPLES "Also check Examples/ for banned headers" OFF)
option(CHARM_ENABLE_TEST_MODULES "Include Modules/*/tests/*.cppm" OFF)
set(CHARM_BANNED_HEADERS "iostream;fstream;filesystem;regex;format;locale;codecvt" CACHE STRING "Semicolon list of banned headers")
set(CHARM_BANNED_HEADERS_EXCLUDE_REGEX "" CACHE STRING "Semicolon list of regexes to exclude from banned header scan")
option(CHARM_FORBID_SHARED_LIBS "Forbid shared/module library targets" OFF)
option(CHARM_ENABLE_ARCH_LINT_GUARD "Run arch_lint guard during build" OFF)

add_library(${target_name})

if (DEFINED CHARM_CONFIG_TARGETS AND CHARM_CONFIG_TARGETS)
    foreach(charm_config_target IN LISTS CHARM_CONFIG_TARGETS)
        if (NOT TARGET ${charm_config_target})
            message(FATAL_ERROR
                "CHARM_CONFIG_TARGETS contains non-target entry: ${charm_config_target}")
        endif()
    endforeach()
    target_link_libraries(${target_name} PRIVATE ${CHARM_CONFIG_TARGETS})
endif()

function(charm_normalize_path_list list_var)
    set(charm_normalized_paths)
    foreach(charm_path IN LISTS ${list_var})
        cmake_path(ABSOLUTE_PATH charm_path NORMALIZE OUTPUT_VARIABLE charm_normalized_path)
        list(APPEND charm_normalized_paths "${charm_normalized_path}")
    endforeach()
    set(${list_var} "${charm_normalized_paths}" PARENT_SCOPE)
endfunction()

target_include_directories(${target_name} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules/io/posix")

#add_compile_options(
target_compile_options(${target_name} PRIVATE
        -Wall
#        -fno-exceptions
#        -fno-rtti
        #        -ffreestanding
        #        -nostdlib
)

set(CHARM_ENTRY_ENFORCE_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules/system/entry/entry_enforce.h")
if (EXISTS "${CHARM_ENTRY_ENFORCE_HEADER}")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_compile_options(
            $<$<COMPILE_LANGUAGE:C,CXX>:-include>
            $<$<COMPILE_LANGUAGE:C,CXX>:${CHARM_ENTRY_ENFORCE_HEADER}>
        )
    elseif (MSVC)
        add_compile_options(
            $<$<COMPILE_LANGUAGE:C,CXX>:/FI"${CHARM_ENTRY_ENFORCE_HEADER}">
        )
    endif()
endif()

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

if (CHARM_ENABLE_FREETYPE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FreeType.cmake")
    charm_link_freetype(${target_name})
endif()

set(CHARM_MATERIAL_COLOR_UTILS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules/thirdparty/material_color_utils")
if (EXISTS "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp"
    AND CHARM_ENABLE_UI_VIVID
    AND NOT CHARM_BAREMETAL)
    target_include_directories(${target_name} PRIVATE "${CHARM_MATERIAL_COLOR_UTILS_DIR}")
    set(MATERIAL_COLOR_UTILS_SOURCES
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/utils/utils.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/cam/cam.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/cam/hct.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/cam/hct_solver.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/cam/viewing_conditions.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/contrast/contrast.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/dislike/dislike.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/dynamiccolor/dynamic_color.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/dynamiccolor/dynamic_scheme.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/dynamiccolor/material_dynamic_colors.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/palettes/tones.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/quantize/celebi.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/quantize/wsmeans.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/quantize/wu.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/quantize/lab.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/score/score.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/scheme/scheme_expressive.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/scheme/scheme_fruit_salad.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/scheme/scheme_tonal_spot.cc"
        "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp/scheme/scheme_vibrant.cc")
    target_sources(${target_name} PRIVATE ${MATERIAL_COLOR_UTILS_SOURCES})
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
    CHARM_LIB_BUILD=1
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

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/control.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/core.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/gfx.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/init.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/io.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/media.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/platform.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/system.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/thirdparty.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/ui.cmake")

set(MODULE_INTERFACE_UNITS
    ${CHARM_CONTROL_MODULES}
    ${CHARM_CORE_MODULES}
    ${CHARM_GFX_MODULES}
    ${CHARM_INIT_MODULES}
    ${CHARM_IO_MODULES}
    ${CHARM_MEDIA_MODULES}
    ${CHARM_PLATFORM_MODULES}
    ${CHARM_SYSTEM_MODULES}
    ${CHARM_THIRDPARTY_MODULES}
    ${CHARM_UI_MODULES})
charm_normalize_path_list(MODULE_INTERFACE_UNITS)
list(REMOVE_DUPLICATES MODULE_INTERFACE_UNITS)
if (NOT WIN32)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/io/net/net.backend.win.cppm"
    )
endif()
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
if (NOT CHARM_ENABLE_TEST_MODULES)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/.*/tests/")
endif()
if (NOT CHARM_ENABLE_POSIX)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/io/posix/")
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/io/net/net.posix.cppm"
    )
endif()
if (NOT CHARM_ENABLE_FREETYPE)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/gfx/font/font_provider_freetype.cppm")
endif()
if (CHARM_BAREMETAL)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/baremetal.cmake")
    charm_normalize_path_list(CHARM_BAREMETAL_EXCLUDED_MODULES)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS ${CHARM_BAREMETAL_EXCLUDED_MODULES})
    foreach(charm_baremetal_exclude_regex IN LISTS CHARM_BAREMETAL_EXCLUDED_REGEXES)
        list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "${charm_baremetal_exclude_regex}")
    endforeach()
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
charm_normalize_path_list(MODULE_INTERFACE_UNITS)
list(REMOVE_DUPLICATES MODULE_INTERFACE_UNITS)
target_sources(${target_name}
        PUBLIC
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS ${MODULE_BASE_DIRS}
        FILES ${MODULE_INTERFACE_UNITS})

if (WIN32)
    target_link_libraries(${target_name} PUBLIC ws2_32)
endif()

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

cmake_minimum_required(VERSION 4.0)
project(Charm-os LANGUAGES C CXX ASM)
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
option(CHARM_BUILD_TARGET_BOOTSTRAP "Build the active leaf target bootstrap image when available" OFF)
set(CHARM_BANNED_HEADERS "iostream;fstream;filesystem;regex;format;locale;codecvt" CACHE STRING "Semicolon list of banned headers")
set(CHARM_BANNED_HEADERS_EXCLUDE_REGEX "" CACHE STRING "Semicolon list of regexes to exclude from banned header scan")
option(CHARM_FORBID_SHARED_LIBS "Forbid shared/module library targets" OFF)
option(CHARM_ENABLE_ARCH_LINT_GUARD "Run arch_lint guard during build" OFF)
set(CHARM_TARGET_NAME "" CACHE STRING "Optional leaf target name under targets/<name>")

set(_CHARM_DEFAULT_STRICT_ALIGNMENT OFF)
if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|ARM)")
    set(_CHARM_DEFAULT_STRICT_ALIGNMENT ON)
endif()
option(CHARM_TARGET_STRICT_ALIGNMENT
    "Target requires conservative codegen for byte-aligned inputs"
    ${_CHARM_DEFAULT_STRICT_ALIGNMENT})
unset(_CHARM_DEFAULT_STRICT_ALIGNMENT)

set(_CHARM_DEFAULT_WINSOCK OFF)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_CHARM_DEFAULT_WINSOCK ON)
endif()
option(CHARM_TARGET_HAS_WINSOCK
    "Target provides WinSock and can build the Windows net backend"
    ${_CHARM_DEFAULT_WINSOCK})
unset(_CHARM_DEFAULT_WINSOCK)

set(_CHARM_DEFAULT_WIN32 OFF)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(_CHARM_DEFAULT_WIN32 ON)
endif()
option(CHARM_TARGET_HAS_WIN32
    "Target provides Win32-style host services and Windows stub backends"
    ${_CHARM_DEFAULT_WIN32})
unset(_CHARM_DEFAULT_WIN32)

set(_CHARM_DEFAULT_HOSTED_CXX ON)
if (CMAKE_SYSTEM_NAME STREQUAL "Generic")
    set(_CHARM_DEFAULT_HOSTED_CXX OFF)
endif()
option(CHARM_TARGET_HAS_HOSTED_CXX
    "Target provides hosted C++ standard library facilities for hosted third-party code"
    ${_CHARM_DEFAULT_HOSTED_CXX})
unset(_CHARM_DEFAULT_HOSTED_CXX)

set(_CHARM_DEFAULT_CXX_MATH ${CHARM_TARGET_HAS_HOSTED_CXX})
option(CHARM_TARGET_HAS_CXX_MATH
    "Target C++ library provides math-related headers such as <cmath>, <complex>, and <numbers>"
    ${_CHARM_DEFAULT_CXX_MATH})
unset(_CHARM_DEFAULT_CXX_MATH)

set(CHARM_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CharmTargetConfig.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/CharmSourceHelpers.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/CharmCoreSources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/CharmIoSources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/CharmPlatformSources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/CharmSystemSources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/CharmMediaSources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sources/CharmUiSources.cmake")

add_library(Charm-build-config INTERFACE)
add_library(Charm::build-config ALIAS Charm-build-config)

add_library(Charm-posix-headers INTERFACE)
target_include_directories(Charm-posix-headers INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules/io/posix")
add_library(Charm::posix-headers ALIAS Charm-posix-headers)

set(CHARM_ACTIVE_TARGET_CONFIG "")
if (NOT CHARM_TARGET_NAME STREQUAL "")
    charm_load_target_config(CHARM_ACTIVE_TARGET_CONFIG "${CHARM_TARGET_NAME}")
    charm_apply_config_targets(Charm-build-config ${CHARM_ACTIVE_TARGET_CONFIG})
    message(STATUS "Charm leaf target: ${CHARM_TARGET_NAME} -> ${CHARM_ACTIVE_TARGET_CONFIG}")
endif()

if (CHARM_BUILD_TARGET_BOOTSTRAP)
    if (NOT CHARM_ACTIVE_TARGET_CONFIG)
        message(FATAL_ERROR
            "CHARM_BUILD_TARGET_BOOTSTRAP=ON requires CHARM_TARGET_NAME to resolve a leaf target config")
    endif()
    charm_add_target_bootstrap(${CHARM_ACTIVE_TARGET_CONFIG})
endif()

target_compile_options(Charm-build-config INTERFACE
    -Wall
)

target_compile_definitions(Charm-build-config INTERFACE
    CHARM_LIB_BUILD=1
    CHARM_ENABLE_MEDIA=$<BOOL:${CHARM_ENABLE_MEDIA}>
    CHARM_ENABLE_UI_INK=$<BOOL:${CHARM_ENABLE_UI_INK}>
    CHARM_ENABLE_UI_VIVID=$<BOOL:${CHARM_ENABLE_UI_VIVID}>
    CHARM_TARGET_HAS_HOSTED_CXX=$<BOOL:${CHARM_TARGET_HAS_HOSTED_CXX}>
    CHARM_TARGET_HAS_CXX_MATH=$<BOOL:${CHARM_TARGET_HAS_CXX_MATH}>
    CHARM_TARGET_HAS_WIN32=$<BOOL:${CHARM_TARGET_HAS_WIN32}>
)

if (CHARM_AUDIO_USE_VFS)
    target_compile_definitions(Charm-build-config INTERFACE CHARM_AUDIO_USE_VFS=1)
endif()
if (CHARM_ENABLE_SDL3)
    target_compile_definitions(Charm-build-config INTERFACE CHARM_ENABLE_SDL3=1)
endif()
if (CHARM_AUDIO_SINK_I2S)
    target_compile_definitions(Charm-build-config INTERFACE CHARM_AUDIO_SINK_I2S=1)
endif()

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

set(CHARM_VIVID_KERNEL "SOA" CACHE STRING "Vivid kernel (fixed to SOA)")
set_property(CACHE CHARM_VIVID_KERNEL PROPERTY STRINGS SOA)
if (DEFINED CHARM_VIVID_SOA_ONLY)
    message(WARNING "CHARM_VIVID_SOA_ONLY is deprecated; Vivid now only supports the SoA kernel.")
endif()
if (DEFINED CHARM_VIVID_PROFILE)
    message(WARNING "CHARM_VIVID_PROFILE is deprecated; Vivid now only supports the SoA kernel.")
endif()

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
endif()

function(charm_add_component_target target)
    add_library(${target} STATIC)
    target_link_libraries(${target} PRIVATE Charm-build-config)
    if (TARGET arch_lint AND CHARM_ENABLE_ARCH_LINT_GUARD)
        add_dependencies(${target} arch_lint)
    endif()
endfunction()

function(charm_attach_module_sources target)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs BASE_DIRS FILES)
    cmake_parse_arguments(CHARM_ATTACH "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT CHARM_ATTACH_FILES)
        return()
    endif()

    target_sources(${target}
        PUBLIC
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS ${CHARM_ATTACH_BASE_DIRS}
        FILES ${CHARM_ATTACH_FILES}
    )
endfunction()

function(charm_add_runtime_facet target alias_name)
    add_library(${target} INTERFACE)
    target_link_libraries(${target} INTERFACE Charm-runtime)
    add_library(${alias_name} ALIAS ${target})
endfunction()

charm_collect_core_sources(CHARM_CORE_MODULES CHARM_CORE_BASE_DIRS)
charm_collect_io_sources(CHARM_IO_MODULES CHARM_IO_BASE_DIRS)
charm_collect_platform_sources(CHARM_PLATFORM_MODULES CHARM_PLATFORM_BASE_DIRS)
charm_collect_system_sources(CHARM_SYSTEM_MODULES CHARM_SYSTEM_BASE_DIRS)

set(CHARM_RUNTIME_MODULES
    ${CHARM_CORE_MODULES}
    ${CHARM_IO_MODULES}
    ${CHARM_PLATFORM_MODULES}
    ${CHARM_SYSTEM_MODULES}
)
set(CHARM_RUNTIME_BASE_DIRS
    ${CHARM_CORE_BASE_DIRS}
    ${CHARM_IO_BASE_DIRS}
    ${CHARM_PLATFORM_BASE_DIRS}
    ${CHARM_SYSTEM_BASE_DIRS}
)
list(REMOVE_DUPLICATES CHARM_RUNTIME_MODULES)
list(REMOVE_DUPLICATES CHARM_RUNTIME_BASE_DIRS)

charm_add_component_target(Charm-runtime)
charm_attach_module_sources(Charm-runtime
    BASE_DIRS ${CHARM_RUNTIME_BASE_DIRS}
    FILES ${CHARM_RUNTIME_MODULES})
add_library(Charm::runtime ALIAS Charm-runtime)

charm_add_runtime_facet(Charm-core Charm::core)
charm_add_runtime_facet(Charm-io Charm::io)
charm_add_runtime_facet(Charm-platform Charm::platform)
charm_add_runtime_facet(Charm-system Charm::system)

if (CHARM_ENABLE_FREETYPE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FreeType.cmake")
    charm_link_freetype(Charm-runtime)
endif()

set(CHARM_MATERIAL_COLOR_UTILS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules/thirdparty/material_color_utils")
if (CHARM_TARGET_HAS_HOSTED_CXX AND EXISTS "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp")
    target_include_directories(Charm-runtime PRIVATE "${CHARM_MATERIAL_COLOR_UTILS_DIR}")
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
    target_sources(Charm-runtime PRIVATE ${MATERIAL_COLOR_UTILS_SOURCES})
elseif (EXISTS "${CHARM_MATERIAL_COLOR_UTILS_DIR}/cpp")
    message(STATUS "Charm-runtime: skipping material_color_utils for non-hosted C++ target")
endif()

if (CHARM_ENABLE_FATFS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FatFs.cmake")
    charm_link_fatfs(Charm-runtime)
endif()

if (CHARM_TARGET_HAS_WINSOCK)
    target_link_libraries(Charm-runtime PUBLIC ws2_32)
endif()

if (CHARM_ENABLE_UI_INK OR CHARM_ENABLE_UI_VIVID)
    charm_add_component_target(Charm-ui)
    target_link_libraries(Charm-ui PUBLIC Charm-runtime)
    charm_collect_ui_sources(Charm-ui CHARM_UI_MODULES CHARM_UI_BASE_DIRS)
    charm_attach_module_sources(Charm-ui
        BASE_DIRS ${CHARM_UI_BASE_DIRS}
        FILES ${CHARM_UI_MODULES})
    add_library(Charm::ui ALIAS Charm-ui)
endif()

if (CHARM_ENABLE_MEDIA)
    charm_add_component_target(Charm-media)
    target_link_libraries(Charm-media PUBLIC Charm-runtime)
    charm_collect_media_sources(CHARM_MEDIA_MODULES CHARM_MEDIA_BASE_DIRS)
    charm_attach_module_sources(Charm-media
        BASE_DIRS ${CHARM_MEDIA_BASE_DIRS}
        FILES ${CHARM_MEDIA_MODULES})
    add_library(Charm::media ALIAS Charm-media)

    if (CHARM_TARGET_STRICT_ALIGNMENT)
        set_source_files_properties(
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/media/audio/audio_decoder_mp3.cppm"
            PROPERTIES COMPILE_OPTIONS "-mno-unaligned-access"
        )
    endif()

    set(CHARM_DR_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Modules/thirdparty/dr_libs")
    if (EXISTS "${CHARM_DR_LIBS_DIR}/dr_mp3.h" AND EXISTS "${CHARM_DR_LIBS_DIR}/dr_flac.h")
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/DRLibs.cmake")
        charm_link_dr_libs(Charm-media)
    endif()

    if (CHARM_ENABLE_SDL3)
        include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SDL3.cmake")
        charm_link_sdl3(Charm-media)
    endif()
endif()

add_library(${target_name} INTERFACE)
add_library(Charm::os ALIAS ${target_name})
if (TARGET arch_lint AND CHARM_ENABLE_ARCH_LINT_GUARD)
    add_dependencies(${target_name} arch_lint)
endif()
target_link_libraries(${target_name} INTERFACE Charm-runtime)
if (CHARM_ACTIVE_TARGET_CONFIG)
    target_link_libraries(${target_name} INTERFACE ${CHARM_ACTIVE_TARGET_CONFIG})
endif()
if (TARGET Charm-ui)
    target_link_libraries(${target_name} INTERFACE Charm-ui)
endif()
if (TARGET Charm-media)
    target_link_libraries(${target_name} INTERFACE Charm-media)
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

if (CHARM_BUILD_PLAYER)
    add_subdirectory(Examples/project/player)
endif()

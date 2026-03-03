cmake_minimum_required(VERSION 4.0)
project(Charm-os)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
option(CHARM_ENABLE_SDL3 "Enable SDL3-backed modules" OFF)
option(CHARM_AUDIO_SINK_I2S "Enable I2S audio sink" OFF)
option(CHARM_ENABLE_MEDIA "Enable media modules" ON)
option(CHARM_ENABLE_UI_INK "Enable Ink UI modules" ON)
option(CHARM_ENABLE_UI_VIVID "Enable Vivid UI modules" ON)
option(CHARM_VIVID_SOA_ONLY "Build Vivid with SoA-only kernel (no ObjectBase/UiFactory)" OFF)
set(CHARM_VIVID_PROFILE "basic" CACHE STRING "Vivid profile: basic or full")
set_property(CACHE CHARM_VIVID_PROFILE PROPERTY STRINGS basic full)
set(CHARM_VIVID_FEATURESET "FULL" CACHE STRING "Vivid featureset: FULL or MCU_MIN")
set_property(CACHE CHARM_VIVID_FEATURESET PROPERTY STRINGS FULL MCU_MIN)
option(CHARM_AUDIO_USE_VFS "Use VFS-backed audio file source" OFF)
option(CHARM_ENABLE_FATFS "Enable FatFs-backed mount" OFF)
option(CHARM_BUILD_PLAYER "Build player example" OFF)
option(CHARM_ENABLE_DEPENDENCY_WHITELIST "Enforce dependency whitelist at configure time" ON)
option(CHARM_ENABLE_BANNED_API_CHECK "Ban printf-family APIs in project code" ON)
option(CHARM_BANNED_API_CHECK_EXAMPLES "Also check Examples/ for banned APIs" OFF)
option(CHARM_ENABLE_BANNED_HEADER_CHECK "Ban selected headers in project code" ON)
option(CHARM_BANNED_HEADER_CHECK_EXAMPLES "Also check Examples/ for banned headers" OFF)
set(CHARM_BANNED_HEADERS "iostream;fstream;filesystem;regex;format;locale;codecvt" CACHE STRING "Semicolon list of banned headers")
set(CHARM_BANNED_HEADERS_EXCLUDE_REGEX "" CACHE STRING "Semicolon list of regexes to exclude from banned header scan")
option(CHARM_FORBID_SHARED_LIBS "Forbid shared/module library targets" ON)

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


if (CHARM_ENABLE_FATFS)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FatFs.cmake")
#    charm_link_fatfs(${target_name})
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
if (CHARM_VIVID_FEATURESET STREQUAL "MCU_MIN")
    target_compile_definitions(${target_name} PRIVATE CHARM_VIVID_FEATURESET_MCU_MIN=1)
elseif (CHARM_VIVID_FEATURESET STREQUAL "FULL")
    target_compile_definitions(${target_name} PRIVATE CHARM_VIVID_FEATURESET_FULL=1)
else()
    message(FATAL_ERROR "Unknown CHARM_VIVID_FEATURESET: ${CHARM_VIVID_FEATURESET}")
endif()
if (CHARM_VIVID_SOA_ONLY)
    target_compile_definitions(${target_name} PRIVATE CHARM_VIVID_SOA_ONLY=1)
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
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/core/alg/alg_text_layout.cppm")
else()
    if (CHARM_VIVID_SOA_ONLY)
        list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/ui/vivid/widgets/")
        list(APPEND MODULE_INTERFACE_UNITS
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/text.cppm")
        list(REMOVE_ITEM MODULE_INTERFACE_UNITS
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/container.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/gui.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/input_router.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/input_interaction.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/layout.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/object.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/factory.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/factory.full.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/virtual_list.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/charm.ui.vivid.full.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/theme_preset.full.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/anim.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/font/font_noto_ascii_16.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/font/font_noto_sc_16.cppm"
        )
    endif()
    if (NOT CHARM_VIVID_SOA_ONLY AND CHARM_VIVID_PROFILE STREQUAL "basic")
        set(CHARM_VIVID_BASIC_WIDGETS
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/button.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/checkbox.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/image.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/image_box.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/label.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/list.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/list_view.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/progress.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/radio.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/scroll_container.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/scroll_dirty.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/scrollbar.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/slider.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/switch.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/text.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/text_area.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/widgets/text_input.cppm"
        )
        list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/ui/vivid/widgets/")
        list(APPEND MODULE_INTERFACE_UNITS ${CHARM_VIVID_BASIC_WIDGETS})
        list(REMOVE_ITEM MODULE_INTERFACE_UNITS
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/charm.ui.vivid.full.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/factory.full.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/theme_preset.full.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/anim.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/font/font_noto_ascii_16.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/font/font_noto_sc_16.cppm")
    elseif (NOT CHARM_VIVID_SOA_ONLY AND CHARM_VIVID_PROFILE STREQUAL "full")
        list(REMOVE_ITEM MODULE_INTERFACE_UNITS
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/factory.cppm"
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules/ui/vivid/core/theme_preset.cppm")
        target_compile_definitions(${target_name} PRIVATE CHARM_VIVID_PROFILE_FULL=1)
    elseif (NOT CHARM_VIVID_SOA_ONLY AND NOT CHARM_VIVID_PROFILE STREQUAL "full")
        message(FATAL_ERROR "Unknown CHARM_VIVID_PROFILE: ${CHARM_VIVID_PROFILE}")
    endif()
endif()
if (NOT CHARM_ENABLE_UI_INK AND NOT CHARM_ENABLE_UI_VIVID)
    list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "/Modules/ui/common/")
endif()
if (NOT CHARM_ENABLE_MEDIA OR NOT CHARM_ENABLE_UI_INK OR NOT CHARM_ENABLE_UI_VIVID)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/charm.domain.cppm")
endif()
target_sources(${target_name}
        PUBLIC
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS
            "${CMAKE_CURRENT_SOURCE_DIR}/Modules"
            "${CMAKE_CURRENT_SOURCE_DIR}/Examples/project/player/stn32common"
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

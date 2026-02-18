cmake_minimum_required(VERSION 4.0)
project(Charm-os)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
option(CHARM_ENABLE_SDL3 "Enable SDL3-backed modules" OFF)
option(CHARM_ENABLE_MEDIA "Enable media modules" ON)
option(CHARM_ENABLE_UI_INK "Enable Ink UI modules" ON)
option(CHARM_ENABLE_UI_VIVID "Enable Vivid UI modules" ON)
option(CHARM_USE_ETL "Use ETL for core util aliases" OFF)
option(CHARM_FETCHCONTENT_ETL "Fetch ETL via FetchContent when not found" ON)

set(CHARM_ETL_GIT_REPOSITORY "https://github.com/ETLCPP/etl.git" CACHE STRING "ETL git repository")
set(CHARM_ETL_GIT_TAG "master" CACHE STRING "ETL git tag")

add_library(${target_name})

#add_compile_options(
target_compile_options(${target_name} PRIVATE
        -Wall
#        -fno-exceptions
#        -fno-rtti
        #        -ffreestanding
        #        -nostdlib
)

if (CHARM_USE_ETL)
    if (NOT DEFINED CHARM_ETL_ROOT)
        set(CHARM_ETL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Modules/thirdparty/etl")
    endif()

    if (EXISTS "${CHARM_ETL_ROOT}/include/etl/etl.h")
        set(CHARM_ETL_INCLUDE "${CHARM_ETL_ROOT}/include")
    elseif (EXISTS "${CHARM_ETL_ROOT}/etl/etl.h")
        set(CHARM_ETL_INCLUDE "${CHARM_ETL_ROOT}")
    elseif (CHARM_FETCHCONTENT_ETL)
        include(FetchContent)
        message(STATUS "Fetching ETL via FetchContent")
        FetchContent_Declare(
            etl
            GIT_REPOSITORY ${CHARM_ETL_GIT_REPOSITORY}
            GIT_TAG ${CHARM_ETL_GIT_TAG}
        )
        FetchContent_MakeAvailable(etl)
        if (EXISTS "${etl_SOURCE_DIR}/include/etl/etl.h")
            set(CHARM_ETL_INCLUDE "${etl_SOURCE_DIR}/include")
        elseif (EXISTS "${etl_SOURCE_DIR}/etl/etl.h")
            set(CHARM_ETL_INCLUDE "${etl_SOURCE_DIR}")
        else()
            message(FATAL_ERROR "ETL headers not found in FetchContent source at ${etl_SOURCE_DIR}")
        endif()
    else()
        message(FATAL_ERROR "CHARM_USE_ETL is ON, but ETL headers were not found under ${CHARM_ETL_ROOT}")
    endif()
    target_include_directories(${target_name} PRIVATE "${CHARM_ETL_INCLUDE}")
    target_compile_definitions(${target_name} PRIVATE CHARM_USE_ETL=1)
endif()


file(GLOB_RECURSE MODULE_INTERFACE_UNITS "Modules/*.cppm")
if (NOT CHARM_ENABLE_SDL3)
    list(REMOVE_ITEM MODULE_INTERFACE_UNITS
        "${CMAKE_CURRENT_SOURCE_DIR}/Modules/media/audio/audio_sink_sdl3.cppm"
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
message(STATUS "${target_name} Examples: Audio/SDL3 -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/audio/sdl3_wav_demo")
message(STATUS "${target_name} Examples: FS/VSF -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/fs")
message(STATUS "${target_name} Examples: Shell/VSF -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/shell/vsf_service_shell")
message(STATUS "${target_name} Examples: Shell/FS+Module -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/shell/vsf_shell_fs_module")
message(STATUS "${target_name} Examples: Service/VSF -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/service/vsf_service_core")
message(STATUS "${target_name} Examples: Service/DS -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/service/service_ds_demo")
message(STATUS "${target_name} Examples: HAL/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/hal/hal_demo")
message(STATUS "${target_name} Examples: Alg/Demo -> ${CMAKE_CURRENT_SOURCE_DIR}/Examples/alg/alg_demo")

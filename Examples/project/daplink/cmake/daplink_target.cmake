include_guard(GLOBAL)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME daplink)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

function(daplink_configure_target)
    set(options)
    set(oneValueArgs TARGET LOCAL_ROOT APP_ROOT STM32CUBEMX_DIR STM32_DEVICE CHARM_ROOT)
    set(multiValueArgs EXTRA_CPP_SOURCES)
    cmake_parse_arguments(DAPLINK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach(required_arg TARGET LOCAL_ROOT APP_ROOT STM32CUBEMX_DIR STM32_DEVICE CHARM_ROOT)
        if(NOT DAPLINK_${required_arg})
            message(FATAL_ERROR "daplink_configure_target(...): ${required_arg} is required")
        endif()
    endforeach()

    set(_module_base_dir "${DAPLINK_APP_ROOT}/..")
    set(_compile_definitions
        CHARM_VIVID_ENABLE_LAYER_CACHE=0
        CHARM_DAP_ENABLE_SWO=0
        CHARM_DAP_ENABLE_SWO_STREAM=0
        CHARM_DAP_ENABLE_DAP_UART=0
        CHARM_DAP_CDC_UART=2
        CHARM_DAP_USB_PROFILE=2
        ${DAPLINK_STM32_DEVICE}
        USE_HAL_DRIVER
        CHARM_KERNEL_REQUIRE_SSU_META=1
    )

    add_executable(${DAPLINK_TARGET})
    add_subdirectory("${DAPLINK_STM32CUBEMX_DIR}")

    file(GLOB_RECURSE _module_interface_units CONFIGURE_DEPENDS
        "${DAPLINK_APP_ROOT}/*.cppm"
        "${_module_base_dir}/frontends/*.cppm"
        "${_module_base_dir}/port/*.cppm"
        "${DAPLINK_LOCAL_ROOT}/*.cppm"
    )
    list(REMOVE_ITEM _module_interface_units
        "${DAPLINK_LOCAL_ROOT}/audio_player_demo.cppm"
        "${DAPLINK_LOCAL_ROOT}/audio_demo.cppm"
    )

    target_sources(${DAPLINK_TARGET} PRIVATE
        "${DAPLINK_APP_ROOT}/main.cpp"
        ${DAPLINK_EXTRA_CPP_SOURCES}
        PUBLIC
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS
            "${_module_base_dir}"
        FILES
            ${_module_interface_units}
    )

    target_include_directories(${DAPLINK_TARGET} PRIVATE
        "${DAPLINK_APP_ROOT}"
        "${_module_base_dir}"
    )

    target_compile_definitions(${DAPLINK_TARGET} PRIVATE
        ${_compile_definitions}
    )

    include("${DAPLINK_CHARM_ROOT}/cmake/CharmTargetConfig.cmake")
    charm_add_config_interface(charm_cfg_daplink_charm
        INCLUDE_DIRECTORIES
            $<TARGET_PROPERTY:stm32cubemx,INTERFACE_INCLUDE_DIRECTORIES>
        COMPILE_DEFINITIONS
            ${_compile_definitions}
    )

    set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
    set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
    set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
    set(CHARM_ENABLE_FATFS OFF CACHE BOOL "" FORCE)
    set(CHARM_AUDIO_SINK_I2S OFF CACHE BOOL "" FORCE)
    set(CHARM_FATFS_ROOT "${DAPLINK_CHARM_ROOT}/Modules/thirdparty/fatfs" CACHE PATH "" FORCE)

    add_subdirectory("${DAPLINK_CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
    charm_apply_config_targets(Charm-build-config charm_cfg_daplink_charm)

    target_link_libraries(${DAPLINK_TARGET}
        stm32cubemx
        Charm-os
    )
endfunction()

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

set(DAPLINK_USB_PROFILE "composite" CACHE STRING "USB profile to build for DAPLink")
set_property(CACHE DAPLINK_USB_PROFILE PROPERTY STRINGS hid cdc composite)
option(DAPLINK_ENABLE_CHARM_INTEGRATION "Build DAPLink with Charm integration enabled" OFF)

function(daplink_configure_target)
    set(options)
    set(oneValueArgs TARGET LOCAL_ROOT APP_ROOT CHARM_ROOT PORT_NAME PORT_LINKER_SCRIPT)
    set(multiValueArgs EXTRA_CPP_SOURCES PORT_TARGET_FLAGS)
    cmake_parse_arguments(DAPLINK "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach(required_arg
        TARGET
        LOCAL_ROOT
        APP_ROOT
        CHARM_ROOT
        PORT_NAME
        PORT_LINKER_SCRIPT
    )
        if(NOT DAPLINK_${required_arg})
            message(FATAL_ERROR "daplink_configure_target(...): ${required_arg} is required")
        endif()
    endforeach()

    if(NOT DAPLINK_PORT_TARGET_FLAGS)
        message(FATAL_ERROR "daplink_configure_target(...): PORT_TARGET_FLAGS is required")
    endif()

    set(_stm32cubemx_dir "${DAPLINK_LOCAL_ROOT}/cmake/stm32cubemx")
    if(NOT EXISTS "${_stm32cubemx_dir}")
        message(FATAL_ERROR
            "Port '${DAPLINK_PORT_NAME}' must provide the CubeMX backend at '${_stm32cubemx_dir}'.")
    endif()

    set(_module_base_dir "${DAPLINK_APP_ROOT}/..")
    if(DAPLINK_USB_PROFILE STREQUAL "hid")
        set(_usb_profile_value 0)
    elseif(DAPLINK_USB_PROFILE STREQUAL "cdc")
        set(_usb_profile_value 1)
    elseif(DAPLINK_USB_PROFILE STREQUAL "composite")
        set(_usb_profile_value 2)
    else()
        message(FATAL_ERROR
            "Unsupported DAPLINK_USB_PROFILE='${DAPLINK_USB_PROFILE}'. "
            "Supported profiles: hid, cdc, composite.")
    endif()

    add_compile_options(
        ${DAPLINK_PORT_TARGET_FLAGS}
        -Wall
        -Wextra
        -Wpedantic
        -fdata-sections
        -ffunction-sections
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Release>:-Os>
        $<$<CONFIG:Release>:-g0>
        $<$<COMPILE_LANGUAGE:ASM>:-x>
        $<$<COMPILE_LANGUAGE:ASM>:assembler-with-cpp>
        $<$<COMPILE_LANGUAGE:ASM>:-MMD>
        $<$<COMPILE_LANGUAGE:ASM>:-MP>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        $<$<COMPILE_LANGUAGE:CXX>:-fno-threadsafe-statics>
    )

    set(_compile_definitions
        DAPLINK_ENABLE_SWO=0
        DAPLINK_ENABLE_SWO_STREAM=0
        DAPLINK_ENABLE_DAP_UART=0
        DAPLINK_CDC_UART_INDEX=2
        DAPLINK_USB_PROFILE_VALUE=${_usb_profile_value}
        USE_HAL_DRIVER
    )

    add_executable(${DAPLINK_TARGET})

    target_link_options(${DAPLINK_TARGET} PRIVATE
        ${DAPLINK_PORT_TARGET_FLAGS}
        -T "${DAPLINK_PORT_LINKER_SCRIPT}"
        --specs=nano.specs
        -Wl,-Map=${CMAKE_PROJECT_NAME}.map
        -Wl,--gc-sections
        -Wl,--start-group
        -lc
        -lm
        -lstdc++
        -lsupc++
        -Wl,--end-group
        -Wl,--print-memory-usage
    )

    add_subdirectory("${_stm32cubemx_dir}")

    get_target_property(_stm32cubemx_compile_definitions stm32cubemx INTERFACE_COMPILE_DEFINITIONS)
    if(NOT _stm32cubemx_compile_definitions)
        message(FATAL_ERROR
            "Port '${DAPLINK_PORT_NAME}' must export INTERFACE_COMPILE_DEFINITIONS from stm32cubemx.")
    endif()

    set(_port_stamp_file "${CMAKE_BINARY_DIR}/daplink.port.stamp")
    string(CONCAT _port_stamp_expected
        "DAPLINK_PORT_NAME=${DAPLINK_PORT_NAME}\n"
        "DAPLINK_PORT_DIR=${DAPLINK_LOCAL_ROOT}\n"
        "DAPLINK_PORT_MANIFEST=${DAPLINK_LOCAL_ROOT}/daplink.port.cmake\n")
    if(EXISTS "${_port_stamp_file}")
        file(READ "${_port_stamp_file}" _port_stamp_current)
        string(REPLACE "\r\n" "\n" _port_stamp_current "${_port_stamp_current}")
        string(REPLACE "\n;" "\n" _port_stamp_current "${_port_stamp_current}")
        string(REGEX REPLACE "^;" "" _port_stamp_current "${_port_stamp_current}")
        if(NOT _port_stamp_current STREQUAL _port_stamp_expected)
            message(FATAL_ERROR
                "Build directory '${CMAKE_BINARY_DIR}' is already bound to another DAPLink port.\n"
                "Expected:\n${_port_stamp_expected}"
                "Found:\n${_port_stamp_current}")
        endif()
    endif()
    file(WRITE "${_port_stamp_file}" "${_port_stamp_expected}")
    set(DAPLINK_ACTIVE_PORT_NAME "${DAPLINK_PORT_NAME}" CACHE STRING "Resolved DAPLink port name" FORCE)
    set(DAPLINK_ACTIVE_PORT_DIR "${DAPLINK_LOCAL_ROOT}" CACHE PATH "Resolved DAPLink port directory" FORCE)
    set(DAPLINK_ACTIVE_PORT_MANIFEST "${DAPLINK_LOCAL_ROOT}/daplink.port.cmake" CACHE FILEPATH "Resolved DAPLink port manifest" FORCE)

    file(GLOB_RECURSE _module_interface_units CONFIGURE_DEPENDS
        "${DAPLINK_APP_ROOT}/*.cppm"
        "${_module_base_dir}/base/*.cppm"
        "${_module_base_dir}/frontends/*.cppm"
        "${_module_base_dir}/io/*.cppm"
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
        ${_stm32cubemx_compile_definitions}
    )

    if(DAPLINK_ENABLE_CHARM_INTEGRATION)
        include("${DAPLINK_CHARM_ROOT}/cmake/CharmTargetConfig.cmake")
        charm_add_config_interface(charm_cfg_daplink_charm
            INCLUDE_DIRECTORIES
                $<TARGET_PROPERTY:stm32cubemx,INTERFACE_INCLUDE_DIRECTORIES>
            COMPILE_DEFINITIONS
                ${_compile_definitions}
                ${_stm32cubemx_compile_definitions}
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
    else()
        target_link_libraries(${DAPLINK_TARGET}
            stm32cubemx
        )
    endif()
endfunction()

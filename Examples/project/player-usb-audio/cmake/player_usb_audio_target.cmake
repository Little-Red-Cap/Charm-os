include_guard(GLOBAL)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME player_usb_audio)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(NOT DEFINED PLAYER_USB_AUDIO_USB_STACK)
    set(PLAYER_USB_AUDIO_USB_STACK "charm_cdc")
endif()

function(player_usb_audio_configure_target)
    set(options)
    set(oneValueArgs TARGET LOCAL_ROOT APP_ROOT STM32CUBEMX_DIR STM32_DEVICE CHARM_ROOT)
    cmake_parse_arguments(PLAYER_USB_AUDIO "${options}" "${oneValueArgs}" "" ${ARGN})

    foreach(required_arg TARGET LOCAL_ROOT APP_ROOT STM32CUBEMX_DIR STM32_DEVICE CHARM_ROOT)
        if(NOT PLAYER_USB_AUDIO_${required_arg})
            message(FATAL_ERROR "player_usb_audio_configure_target(...): ${required_arg} is required")
        endif()
    endforeach()

    set(_compile_definitions
        CHARM_ALLOW_HAL=1
        CHARM_TARGET_SOC_STM32G431=1
        CHARM_TARGET_BOARD_STM32G431_PLAYER_AUDIO=1
        CHARM_TARGET_CPU_CORTEX_M4=1
        CHARM_STM32G431_PLAYER_AUDIO_HSE_HZ=8000000
        ${PLAYER_USB_AUDIO_STM32_DEVICE}
        USE_HAL_DRIVER
    )

    add_executable(${PLAYER_USB_AUDIO_TARGET})
    add_subdirectory("${PLAYER_USB_AUDIO_STM32CUBEMX_DIR}")

    if(PLAYER_USB_AUDIO_USB_STACK STREQUAL "charm_cdc")
        target_sources(${PLAYER_USB_AUDIO_TARGET} PRIVATE
            "${PLAYER_USB_AUDIO_APP_ROOT}/main.cpp"
        )
    elseif(PLAYER_USB_AUDIO_USB_STACK STREQUAL "st_uac")
        target_sources(${PLAYER_USB_AUDIO_TARGET} PRIVATE
            "${PLAYER_USB_AUDIO_APP_ROOT}/main_uac_st.cpp"
        )
        add_subdirectory("${PLAYER_USB_AUDIO_LOCAL_ROOT}/USB_DEVICE")
        target_link_libraries(${PLAYER_USB_AUDIO_TARGET} PRIVATE usb_device_audio_st)
    else()
        message(FATAL_ERROR
            "Unsupported PLAYER_USB_AUDIO_USB_STACK='${PLAYER_USB_AUDIO_USB_STACK}'. "
            "Supported values: charm_cdc, st_uac.")
    endif()

    target_compile_definitions(${PLAYER_USB_AUDIO_TARGET} PRIVATE
        ${_compile_definitions}
    )

    if(PLAYER_USB_AUDIO_USB_STACK STREQUAL "charm_cdc")
        include("${PLAYER_USB_AUDIO_CHARM_ROOT}/cmake/CharmTargetConfig.cmake")
        charm_add_config_interface(charm_cfg_player_usb_audio
            ARCH armv7e_m
            PLATFORM stm32g431_player_audio
            BAREMETAL
            INCLUDE_DIRECTORIES
                $<TARGET_PROPERTY:stm32cubemx,INTERFACE_INCLUDE_DIRECTORIES>
            COMPILE_DEFINITIONS
                ${_compile_definitions}
        )

        set(CHARM_TARGET_NAME "" CACHE STRING "" FORCE)
        set(CHARM_BUILD_TARGET_BOOTSTRAP OFF CACHE BOOL "" FORCE)
        set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
        set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
        set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
        set(CHARM_ENABLE_FATFS OFF CACHE BOOL "" FORCE)
        set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
        set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
        set(CHARM_AUDIO_SINK_I2S OFF CACHE BOOL "" FORCE)
        set(CHARM_AUDIO_USE_VFS OFF CACHE BOOL "" FORCE)
        set(CHARM_TARGET_HAS_HOSTED_CXX OFF CACHE BOOL "" FORCE)
        set(CHARM_TARGET_HAS_CXX_MATH OFF CACHE BOOL "" FORCE)
        set(CHARM_TARGET_HAS_WIN32 OFF CACHE BOOL "" FORCE)
        set(CHARM_TARGET_HAS_WINSOCK OFF CACHE BOOL "" FORCE)
        set(CHARM_TARGET_STRICT_ALIGNMENT ON CACHE BOOL "" FORCE)

        add_subdirectory("${PLAYER_USB_AUDIO_CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
        charm_apply_config_targets(Charm-build-config charm_cfg_player_usb_audio)

        target_link_libraries(${PLAYER_USB_AUDIO_TARGET} PRIVATE
            stm32cubemx
            Charm-os
        )
    else()
        target_link_libraries(${PLAYER_USB_AUDIO_TARGET} PRIVATE stm32cubemx)
    endif()
endfunction()

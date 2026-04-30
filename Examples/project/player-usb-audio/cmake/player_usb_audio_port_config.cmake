include_guard(GLOBAL)

function(player_usb_audio_resolve_port PORT_NAME)
    set(_port_config_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")

    if(PORT_NAME STREQUAL "g431")
        set(_local_root "${_port_config_dir}/../g431")
        set(_cubemx_dir "g431/cmake/stm32cubemx")
        set(_device "STM32G431xx")
    else()
        message(FATAL_ERROR
            "Unsupported PLAYER_USB_AUDIO_PORT='${PORT_NAME}'. "
            "Supported ports: g431.")
    endif()

    set(PLAYER_USB_AUDIO_PORT_LOCAL_ROOT "${_local_root}" PARENT_SCOPE)
    set(PLAYER_USB_AUDIO_PORT_STM32CUBEMX_DIR "${_cubemx_dir}" PARENT_SCOPE)
    set(PLAYER_USB_AUDIO_PORT_STM32_DEVICE "${_device}" PARENT_SCOPE)
endfunction()

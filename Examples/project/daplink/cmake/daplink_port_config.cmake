include_guard(GLOBAL)

function(daplink_resolve_port PORT_NAME)
    set(_port_config_dir "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")

    if(PORT_NAME STREQUAL "f103")
        set(_local_root "${_port_config_dir}/../f103")
        set(_cubemx_dir "f103/cmake/stm32cubemx")
        set(_device "STM32F103xx")
    elseif(PORT_NAME STREQUAL "g431")
        set(_local_root "${_port_config_dir}/../g431")
        set(_cubemx_dir "g431/cmake/stm32cubemx")
        set(_device "STM32G431xx")
    elseif(PORT_NAME STREQUAL "h503")
        set(_local_root "${_port_config_dir}/../h503")
        set(_cubemx_dir "h503/cmake/stm32cubemx")
        set(_device "STM32H503xx")
    else()
        message(FATAL_ERROR
            "Unsupported DAPLINK_PORT='${PORT_NAME}'. "
            "Supported ports: f103, g431, h503.")
    endif()

    set(_extra_cpp_sources "")
    if(EXISTS "${_local_root}/daplink_port_usb_callbacks.cpp")
        list(APPEND _extra_cpp_sources "${_local_root}/daplink_port_usb_callbacks.cpp")
    endif()

    set(DAPLINK_PORT_LOCAL_ROOT "${_local_root}" PARENT_SCOPE)
    set(DAPLINK_PORT_STM32CUBEMX_DIR "${_cubemx_dir}" PARENT_SCOPE)
    set(DAPLINK_PORT_STM32_DEVICE "${_device}" PARENT_SCOPE)
    set(DAPLINK_PORT_EXTRA_CPP_SOURCES "${_extra_cpp_sources}" PARENT_SCOPE)
endfunction()

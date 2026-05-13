include_guard(GLOBAL)

function(daplink_require_stm32cube_root series out_var)
    string(TOUPPER "${series}" _series_upper)
    set(_var_name "DAPLINK_STM32CUBE_${_series_upper}_ROOT")
    set(_root_value "")

    if(DEFINED ${_var_name} AND NOT "${${_var_name}}" STREQUAL "")
        set(_root_value "${${_var_name}}")
    elseif(DEFINED ENV{${_var_name}} AND NOT "$ENV{${_var_name}}" STREQUAL "")
        set(_root_value "$ENV{${_var_name}}")
    endif()

    if("${_root_value}" STREQUAL "")
        message(FATAL_ERROR
            "Missing ${_var_name}.\n"
            "Set it as a CMake cache variable or environment variable and point it to the STM32Cube firmware package root for series '${series}'.\n"
            "Example expected layout:\n"
            "  <root>/Drivers/STM32${_series_upper}xx_HAL_Driver\n"
            "  <root>/Drivers/CMSIS")
    endif()

    get_filename_component(_root "${_root_value}" ABSOLUTE)
    if(NOT EXISTS "${_root}/Drivers")
        message(FATAL_ERROR
            "${_var_name}='${_root}' does not look like a STM32Cube package root.\n"
            "Expected '${_root}/Drivers' to exist.")
    endif()

    set(${out_var} "${_root}" PARENT_SCOPE)
endfunction()

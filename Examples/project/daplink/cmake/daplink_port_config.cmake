include_guard(GLOBAL)

function(daplink_resolve_port_dir PORT_DIR OUT_DIR OUT_MANIFEST)
    if(NOT PORT_DIR)
        set(_port_stamp_file "${CMAKE_BINARY_DIR}/daplink.port.stamp")
        if(EXISTS "${_port_stamp_file}")
            file(READ "${_port_stamp_file}" _port_stamp_current)
            string(REPLACE "\r\n" "\n" _port_stamp_current "${_port_stamp_current}")
            string(REPLACE "\n;" "\n" _port_stamp_current "${_port_stamp_current}")
            string(REGEX REPLACE "^;" "" _port_stamp_current "${_port_stamp_current}")
            string(REGEX MATCH "DAPLINK_PORT_DIR=([^\n]+)" _port_stamp_port_dir "${_port_stamp_current}")
            if(_port_stamp_port_dir)
                set(PORT_DIR "${CMAKE_MATCH_1}")
            endif()
        endif()

        if(NOT PORT_DIR)
            set(_port_cache_file "${CMAKE_BINARY_DIR}/CMakeCache.txt")
            if(EXISTS "${_port_cache_file}")
                file(READ "${_port_cache_file}" _port_cache_current)
                string(REPLACE "\r\n" "\n" _port_cache_current "${_port_cache_current}")
                string(REGEX MATCH "DAPLINK_ACTIVE_PORT_DIR:PATH=([^\n]+)" _port_cache_active_port_dir "${_port_cache_current}")
                if(_port_cache_active_port_dir)
                    set(PORT_DIR "${CMAKE_MATCH_1}")
                else()
                    string(REGEX MATCH "DAPLINK_PORT_DIR:PATH=([^\n]+)" _port_cache_port_dir "${_port_cache_current}")
                    if(_port_cache_port_dir)
                        set(PORT_DIR "${CMAKE_MATCH_1}")
                    endif()
                endif()
            endif()
        endif()

        if(NOT PORT_DIR AND DEFINED DAPLINK_DEFAULT_PORT_DIR AND NOT "${DAPLINK_DEFAULT_PORT_DIR}" STREQUAL "")
            set(PORT_DIR "${DAPLINK_DEFAULT_PORT_DIR}")
        endif()

        if(NOT PORT_DIR)
            message(FATAL_ERROR
                "DAPLINK_PORT_DIR is required on the first configure. "
                "Point it to a concrete port directory such as '${CMAKE_CURRENT_SOURCE_DIR}/g431'.")
        endif()
    endif()

    get_filename_component(_resolved_port_dir "${PORT_DIR}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    set(_manifest_path "${_resolved_port_dir}/daplink.port.cmake")

    if(NOT EXISTS "${_resolved_port_dir}")
        message(FATAL_ERROR "DAPLINK_PORT_DIR does not exist: ${_resolved_port_dir}")
    endif()

    if(NOT EXISTS "${_manifest_path}")
        message(FATAL_ERROR
            "DAPLINK port manifest not found: ${_manifest_path}. "
            "Each port directory must provide 'daplink.port.cmake'.")
    endif()

    set(${OUT_DIR} "${_resolved_port_dir}" PARENT_SCOPE)
    set(${OUT_MANIFEST} "${_manifest_path}" PARENT_SCOPE)
endfunction()

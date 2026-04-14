function(charm_add_config_interface target)
    set(options BAREMETAL POSIX_HEADERS)
    set(oneValueArgs ARCH PLATFORM)
    set(multiValueArgs COMPILE_DEFINITIONS INCLUDE_DIRECTORIES COMPILE_OPTIONS LINK_OPTIONS LINK_LIBRARIES)
    cmake_parse_arguments(CHARM_CFG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (TARGET ${target})
        return()
    endif()

    add_library(${target} INTERFACE)

    if (CHARM_CFG_ARCH)
        string(TOUPPER "${CHARM_CFG_ARCH}" CHARM_CFG_ARCH_UPPER)
        target_compile_definitions(${target} INTERFACE
            "CHARM_ARCH_${CHARM_CFG_ARCH_UPPER}=1")
    endif()

    if (CHARM_CFG_PLATFORM)
        string(TOUPPER "${CHARM_CFG_PLATFORM}" CHARM_CFG_PLATFORM_UPPER)
        string(REPLACE "-" "_" CHARM_CFG_PLATFORM_UPPER "${CHARM_CFG_PLATFORM_UPPER}")
        target_compile_definitions(${target} INTERFACE
            "CHARM_PLATFORM_${CHARM_CFG_PLATFORM_UPPER}=1")
    endif()

    if (CHARM_CFG_BAREMETAL)
        target_compile_definitions(${target} INTERFACE CHARM_BAREMETAL=1)
    endif()

    if (CHARM_CFG_POSIX_HEADERS)
        if (TARGET Charm-posix-headers)
            target_link_libraries(${target} INTERFACE Charm-posix-headers)
        else()
            get_filename_component(CHARM_CFG_ROOT "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
            target_include_directories(${target} INTERFACE
                "${CHARM_CFG_ROOT}/Modules/io/posix")
        endif()
    endif()

    if (CHARM_CFG_COMPILE_DEFINITIONS)
        target_compile_definitions(${target} INTERFACE ${CHARM_CFG_COMPILE_DEFINITIONS})
    endif()

    if (CHARM_CFG_INCLUDE_DIRECTORIES)
        target_include_directories(${target} INTERFACE ${CHARM_CFG_INCLUDE_DIRECTORIES})
    endif()

    if (CHARM_CFG_COMPILE_OPTIONS)
        target_compile_options(${target} INTERFACE ${CHARM_CFG_COMPILE_OPTIONS})
    endif()

    if (CHARM_CFG_LINK_OPTIONS)
        target_link_options(${target} INTERFACE ${CHARM_CFG_LINK_OPTIONS})
    endif()

    if (CHARM_CFG_LINK_LIBRARIES)
        target_link_libraries(${target} INTERFACE ${CHARM_CFG_LINK_LIBRARIES})
    endif()
endfunction()

function(charm_resolve_config_target out_var target)
    set(_resolved_target "${target}")
    if ((target STREQUAL "Charm-os" OR target STREQUAL "Charm::os")
        AND TARGET Charm-build-config)
        set(_resolved_target "Charm-build-config")
    endif()
    set(${out_var} "${_resolved_target}" PARENT_SCOPE)
endfunction()

function(charm_apply_config_targets target)
    charm_resolve_config_target(_resolved_target "${target}")

    if (NOT TARGET ${_resolved_target})
        message(FATAL_ERROR
            "charm_apply_config_targets(${target} ...): missing target ${_resolved_target}")
    endif()

    get_target_property(_target_type ${_resolved_target} TYPE)
    if (_target_type STREQUAL "INTERFACE_LIBRARY")
        set(_link_scope INTERFACE)
    else()
        set(_link_scope PRIVATE)
    endif()

    foreach(cfg IN LISTS ARGN)
        if (NOT TARGET ${cfg})
            message(FATAL_ERROR
                "charm_apply_config_targets(${target} ...): missing config target ${cfg}")
        endif()
        target_link_libraries(${_resolved_target} ${_link_scope} ${cfg})
    endforeach()
endfunction()

function(charm_apply_target_profile target)
    set(options BAREMETAL)
    set(oneValueArgs ARCH)
    cmake_parse_arguments(CHARM_PROFILE "${options}" "${oneValueArgs}" "" ${ARGN})

    if (CHARM_PROFILE_ARCH STREQUAL "aarch64")
        target_compile_options(${target} PRIVATE
            -ffreestanding
            -fno-exceptions
            -fno-rtti
            -fno-threadsafe-statics
            -fno-use-cxa-atexit)
    endif()

    if (CHARM_PROFILE_BAREMETAL)
        target_link_options(${target} PRIVATE
            -nostdlib
            -nostartfiles)
    endif()
endfunction()

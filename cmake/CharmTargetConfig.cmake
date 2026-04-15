function(charm_normalize_config_token out_var value)
    string(TOUPPER "${value}" _upper)
    string(REGEX REPLACE "[^A-Z0-9]" "_" _normalized "${_upper}")
    string(REGEX REPLACE "_+" "_" _normalized "${_normalized}")
    string(REGEX REPLACE "^_" "" _normalized "${_normalized}")
    string(REGEX REPLACE "_$" "" _normalized "${_normalized}")
    set(${out_var} "${_normalized}" PARENT_SCOPE)
endfunction()

function(charm_normalize_target_name out_var value)
    string(TOLOWER "${value}" _lower)
    string(REGEX REPLACE "[^a-z0-9]" "_" _normalized "${_lower}")
    string(REGEX REPLACE "_+" "_" _normalized "${_normalized}")
    string(REGEX REPLACE "^_" "" _normalized "${_normalized}")
    string(REGEX REPLACE "_$" "" _normalized "${_normalized}")
    if (NOT _normalized)
        message(FATAL_ERROR "Unable to normalize target name from '${value}'")
    endif()
    set(${out_var} "${_normalized}" PARENT_SCOPE)
endfunction()

function(charm_get_source_root out_var)
    if (DEFINED CHARM_SOURCE_ROOT AND NOT CHARM_SOURCE_ROOT STREQUAL "")
        set(_root "${CHARM_SOURCE_ROOT}")
    elseif (DEFINED CHARM_ROOT AND NOT CHARM_ROOT STREQUAL "")
        set(_root "${CHARM_ROOT}")
    else()
        get_filename_component(_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/.." ABSOLUTE)
    endif()
    set(${out_var} "${_root}" PARENT_SCOPE)
endfunction()

function(charm_get_target_config_name out_var target_name)
    charm_normalize_target_name(_normalized "${target_name}")
    set(${out_var} "Charm-target-${_normalized}" PARENT_SCOPE)
endfunction()

function(charm_get_target_config_path out_var target_name)
    charm_get_source_root(_root)
    charm_normalize_target_name(_normalized "${target_name}")
    set(${out_var} "${_root}/targets/${_normalized}/CharmTargetConfig.cmake" PARENT_SCOPE)
endfunction()

function(charm_load_target_config out_var target_name)
    if (target_name STREQUAL "")
        message(FATAL_ERROR "charm_load_target_config(...): target name must not be empty")
    endif()

    charm_get_target_config_path(_config_path "${target_name}")
    if (NOT EXISTS "${_config_path}")
        message(FATAL_ERROR
            "charm_load_target_config(...): missing target config '${_config_path}'")
    endif()

    include("${_config_path}")

    charm_get_target_config_name(_config_target "${target_name}")
    if (NOT TARGET ${_config_target})
        message(FATAL_ERROR
            "charm_load_target_config(...): '${_config_path}' did not create target ${_config_target}")
    endif()

    set(${out_var} "${_config_target}" PARENT_SCOPE)
endfunction()

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
        charm_normalize_config_token(CHARM_CFG_ARCH_UPPER "${CHARM_CFG_ARCH}")
        target_compile_definitions(${target} INTERFACE
            "CHARM_ARCH_${CHARM_CFG_ARCH_UPPER}=1")
    endif()

    if (CHARM_CFG_PLATFORM)
        charm_normalize_config_token(CHARM_CFG_PLATFORM_UPPER "${CHARM_CFG_PLATFORM}")
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

function(charm_register_target_bootstrap target)
    set(options)
    set(oneValueArgs PREFIX SOURCES_FILE TARGET_NAME ARCH)
    cmake_parse_arguments(CHARM_BOOT "${options}" "${oneValueArgs}" "" ${ARGN})

    if (NOT TARGET ${target})
        message(FATAL_ERROR
            "charm_register_target_bootstrap(${target} ...): missing config target")
    endif()
    if (NOT CHARM_BOOT_PREFIX)
        message(FATAL_ERROR
            "charm_register_target_bootstrap(${target} ...): PREFIX is required")
    endif()
    if (NOT CHARM_BOOT_SOURCES_FILE)
        message(FATAL_ERROR
            "charm_register_target_bootstrap(${target} ...): SOURCES_FILE is required")
    endif()

    set_property(TARGET ${target} PROPERTY CHARM_BOOTSTRAP_PREFIX "${CHARM_BOOT_PREFIX}")
    set_property(TARGET ${target} PROPERTY CHARM_BOOTSTRAP_SOURCES_FILE "${CHARM_BOOT_SOURCES_FILE}")

    if (CHARM_BOOT_TARGET_NAME)
        set_property(TARGET ${target} PROPERTY CHARM_BOOTSTRAP_TARGET_NAME "${CHARM_BOOT_TARGET_NAME}")
    endif()

    if (CHARM_BOOT_ARCH)
        set_property(TARGET ${target} PROPERTY CHARM_BOOTSTRAP_ARCH "${CHARM_BOOT_ARCH}")
    endif()
endfunction()

function(charm_collect_target_bootstrap_assets
    out_name
    out_sources
    out_include_dirs
    out_compile_defs
    out_compile_options
    out_link_options
    out_link_libraries
    out_depends
    out_linker_script
    out_arch
    config_target)
    if (NOT TARGET ${config_target})
        message(FATAL_ERROR
            "charm_collect_target_bootstrap_assets(...): missing config target ${config_target}")
    endif()

    get_target_property(_prefix ${config_target} CHARM_BOOTSTRAP_PREFIX)
    get_target_property(_sources_file ${config_target} CHARM_BOOTSTRAP_SOURCES_FILE)
    get_target_property(_target_name ${config_target} CHARM_BOOTSTRAP_TARGET_NAME)
    get_target_property(_arch ${config_target} CHARM_BOOTSTRAP_ARCH)

    if ((NOT _prefix) OR _prefix STREQUAL "NOTFOUND")
        message(FATAL_ERROR
            "charm_collect_target_bootstrap_assets(...): ${config_target} has no registered bootstrap prefix")
    endif()
    if ((NOT _sources_file) OR _sources_file STREQUAL "NOTFOUND")
        message(FATAL_ERROR
            "charm_collect_target_bootstrap_assets(...): ${config_target} has no registered bootstrap sources file")
    endif()

    include("${_sources_file}")

    if ((NOT _target_name) OR _target_name STREQUAL "NOTFOUND")
        string(TOLOWER "${_prefix}" _target_name)
        set(_target_name "${_target_name}_bootstrap")
    endif()

    set(_sources_var "${_prefix}_ALL_SOURCES")
    set(_include_dirs_var "${_prefix}_INCLUDE_DIRECTORIES")
    set(_compile_defs_var "${_prefix}_COMPILE_DEFINITIONS")
    set(_compile_options_var "${_prefix}_COMPILE_OPTIONS")
    set(_link_options_var "${_prefix}_LINK_OPTIONS")
    set(_link_libraries_var "${_prefix}_BOOTSTRAP_LINK_LIBRARIES")
    set(_depends_var "${_prefix}_DEPENDS")
    set(_linker_script_var "${_prefix}_LINKER_SCRIPT")

    set(_sources "")
    set(_include_dirs "")
    set(_compile_defs "")
    set(_compile_options "")
    set(_link_options "")
    set(_link_libraries "")
    set(_depends "")
    set(_linker_script "")

    if (DEFINED ${_sources_var})
        set(_sources "${${_sources_var}}")
    endif()
    if (DEFINED ${_include_dirs_var})
        set(_include_dirs "${${_include_dirs_var}}")
    endif()
    if (DEFINED ${_compile_defs_var})
        set(_compile_defs "${${_compile_defs_var}}")
    endif()
    if (DEFINED ${_compile_options_var})
        set(_compile_options "${${_compile_options_var}}")
    endif()
    if (DEFINED ${_link_options_var})
        set(_link_options "${${_link_options_var}}")
    endif()
    if (DEFINED ${_link_libraries_var})
        set(_link_libraries "${${_link_libraries_var}}")
    endif()
    if (DEFINED ${_depends_var})
        set(_depends "${${_depends_var}}")
    endif()
    if (DEFINED ${_linker_script_var})
        set(_linker_script "${${_linker_script_var}}")
    endif()

    set(${out_name} "${_target_name}" PARENT_SCOPE)
    set(${out_sources} "${_sources}" PARENT_SCOPE)
    set(${out_include_dirs} "${_include_dirs}" PARENT_SCOPE)
    set(${out_compile_defs} "${_compile_defs}" PARENT_SCOPE)
    set(${out_compile_options} "${_compile_options}" PARENT_SCOPE)
    set(${out_link_options} "${_link_options}" PARENT_SCOPE)
    set(${out_link_libraries} "${_link_libraries}" PARENT_SCOPE)
    set(${out_depends} "${_depends}" PARENT_SCOPE)
    set(${out_linker_script} "${_linker_script}" PARENT_SCOPE)
    set(${out_arch} "${_arch}" PARENT_SCOPE)
endfunction()

function(charm_add_leaf_image target)
    set(options BAREMETAL)
    set(oneValueArgs CONFIG_TARGET ARCH LINKER_SCRIPT)
    set(multiValueArgs
        SOURCES
        INCLUDE_DIRECTORIES
        COMPILE_DEFINITIONS
        COMPILE_OPTIONS
        LINK_OPTIONS
        LINK_LIBRARIES
        DEPENDS)
    cmake_parse_arguments(CHARM_IMAGE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (TARGET ${target})
        return()
    endif()

    if (NOT CHARM_IMAGE_SOURCES)
        message(FATAL_ERROR
            "charm_add_leaf_image(${target} ...): SOURCES must not be empty")
    endif()

    if (CHARM_IMAGE_CONFIG_TARGET AND NOT TARGET ${CHARM_IMAGE_CONFIG_TARGET})
        message(FATAL_ERROR
            "charm_add_leaf_image(${target} ...): missing config target ${CHARM_IMAGE_CONFIG_TARGET}")
    endif()

    enable_language(ASM)
    add_executable(${target})
    target_sources(${target} PRIVATE ${CHARM_IMAGE_SOURCES})

    if (CHARM_IMAGE_CONFIG_TARGET)
        target_link_libraries(${target} PRIVATE ${CHARM_IMAGE_CONFIG_TARGET})
    endif()

    if (CHARM_IMAGE_INCLUDE_DIRECTORIES)
        target_include_directories(${target} PRIVATE ${CHARM_IMAGE_INCLUDE_DIRECTORIES})
    endif()
    if (CHARM_IMAGE_COMPILE_DEFINITIONS)
        target_compile_definitions(${target} PRIVATE ${CHARM_IMAGE_COMPILE_DEFINITIONS})
    endif()
    if (CHARM_IMAGE_COMPILE_OPTIONS)
        target_compile_options(${target} PRIVATE ${CHARM_IMAGE_COMPILE_OPTIONS})
    endif()
    if (CHARM_IMAGE_DEPENDS)
        add_dependencies(${target} ${CHARM_IMAGE_DEPENDS})
    endif()

    set(_leaf_image_link_options
        -Wl,--build-id=none
        -Wl,--gc-sections
        "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${target}.map")
    if (CHARM_IMAGE_LINKER_SCRIPT)
        list(APPEND _leaf_image_link_options "-T${CHARM_IMAGE_LINKER_SCRIPT}")
    endif()
    if (CHARM_IMAGE_LINK_OPTIONS)
        list(APPEND _leaf_image_link_options ${CHARM_IMAGE_LINK_OPTIONS})
    endif()
    target_link_options(${target} PRIVATE ${_leaf_image_link_options})

    if (CHARM_IMAGE_LINK_LIBRARIES)
        target_link_libraries(${target} PRIVATE ${CHARM_IMAGE_LINK_LIBRARIES})
    endif()

    if (CHARM_IMAGE_ARCH OR CHARM_IMAGE_BAREMETAL)
        set(_profile_args)
        if (CHARM_IMAGE_ARCH)
            list(APPEND _profile_args ARCH ${CHARM_IMAGE_ARCH})
        endif()
        if (CHARM_IMAGE_BAREMETAL)
            list(APPEND _profile_args BAREMETAL)
        endif()
        charm_apply_target_profile(${target} ${_profile_args})
    endif()

    if (DEFINED CMAKE_OBJCOPY AND NOT CMAKE_OBJCOPY STREQUAL "")
        add_custom_command(TARGET ${target} POST_BUILD
            BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/${target}.bin
            COMMAND ${CMAKE_OBJCOPY}
                -O binary
                $<TARGET_FILE:${target}>
                ${CMAKE_CURRENT_BINARY_DIR}/${target}.bin
            VERBATIM)
    endif()
endfunction()

function(charm_add_target_bootstrap config_target)
    set(options)
    set(oneValueArgs TARGET_NAME)
    cmake_parse_arguments(CHARM_BOOT "${options}" "${oneValueArgs}" "" ${ARGN})

    charm_collect_target_bootstrap_assets(
        _bootstrap_name
        _bootstrap_sources
        _bootstrap_include_dirs
        _bootstrap_compile_defs
        _bootstrap_compile_options
        _bootstrap_link_options
        _bootstrap_link_libraries
        _bootstrap_depends
        _bootstrap_linker_script
        _bootstrap_arch
        ${config_target})

    if (CHARM_BOOT_TARGET_NAME)
        set(_bootstrap_name "${CHARM_BOOT_TARGET_NAME}")
    endif()

    charm_add_leaf_image(${_bootstrap_name}
        CONFIG_TARGET ${config_target}
        ARCH ${_bootstrap_arch}
        BAREMETAL
        SOURCES ${_bootstrap_sources}
        INCLUDE_DIRECTORIES ${_bootstrap_include_dirs}
        COMPILE_DEFINITIONS ${_bootstrap_compile_defs}
        COMPILE_OPTIONS ${_bootstrap_compile_options}
        LINK_OPTIONS ${_bootstrap_link_options}
        LINK_LIBRARIES ${_bootstrap_link_libraries}
        DEPENDS ${_bootstrap_depends}
        LINKER_SCRIPT ${_bootstrap_linker_script})
endfunction()

function(charm_apply_target_profile target)
    set(options BAREMETAL)
    set(oneValueArgs ARCH)
    cmake_parse_arguments(CHARM_PROFILE "${options}" "${oneValueArgs}" "" ${ARGN})

    if (CHARM_PROFILE_ARCH STREQUAL "aarch64"
        OR CHARM_PROFILE_ARCH STREQUAL "armv7-a")
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

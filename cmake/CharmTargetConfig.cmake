function(charm_add_config_interface target)
    set(options BAREMETAL)
    set(oneValueArgs ARCH PLATFORM)
    cmake_parse_arguments(CHARM_CFG "${options}" "${oneValueArgs}" "" ${ARGN})

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

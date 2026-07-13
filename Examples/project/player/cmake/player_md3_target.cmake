include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/player_md3_sources.cmake")

function(player_add_md3_component target_name)
    set(options)
    set(one_value_args)
    set(multi_value_args CONFIG_TARGETS)
    cmake_parse_arguments(PLAYER_MD3
        "${options}"
        "${one_value_args}"
        "${multi_value_args}"
        ${ARGN})

    if (TARGET ${target_name})
        message(FATAL_ERROR "Player MD3 component already exists: ${target_name}")
    endif()
    if (NOT TARGET Charm-os)
        message(FATAL_ERROR
            "player_add_md3_component(${target_name}) requires Charm-os to be configured first")
    endif()

    add_library(${target_name} STATIC)
    target_compile_features(${target_name} PUBLIC cxx_std_26)
    target_sources(${target_name}
        PUBLIC
        FILE_SET player_md3_modules TYPE CXX_MODULES
        BASE_DIRS "${CHARM_ROOT}"
        FILES ${PLAYER_MD3_CANONICAL_MODULES}
    )
    target_link_libraries(${target_name} PUBLIC Charm-os)

    if (PLAYER_MD3_CONFIG_TARGETS)
        charm_apply_config_targets(${target_name} ${PLAYER_MD3_CONFIG_TARGETS})
    endif()

    if (target_name STREQUAL "charm_player_md3" AND NOT TARGET Charm::player-md3)
        add_library(Charm::player-md3 ALIAS ${target_name})
    endif()
endfunction()

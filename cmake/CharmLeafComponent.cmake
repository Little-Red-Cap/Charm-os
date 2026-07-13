include_guard(GLOBAL)

function(charm_add_leaf_component target_name)
    set(options)
    set(one_value_args)
    set(multi_value_args BASE_DIRS FILES LINK_LIBRARIES)
    cmake_parse_arguments(LEAF
        "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if (TARGET ${target_name})
        message(FATAL_ERROR "Leaf component already exists: ${target_name}")
    endif()
    add_library(${target_name} STATIC)
    target_compile_features(${target_name} PUBLIC cxx_std_26)
    if (LEAF_FILES)
        target_sources(${target_name}
            PUBLIC
            FILE_SET leaf_modules TYPE CXX_MODULES
            BASE_DIRS ${LEAF_BASE_DIRS}
            FILES ${LEAF_FILES})
    endif()
    if (LEAF_LINK_LIBRARIES)
        target_link_libraries(${target_name} PUBLIC ${LEAF_LINK_LIBRARIES})
    endif()
endfunction()

function(charm_attach_leaf_modules target_name)
    set(options)
    set(one_value_args)
    set(multi_value_args BASE_DIRS FILES)
    cmake_parse_arguments(LEAF
        "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})
    if (NOT TARGET ${target_name} OR NOT LEAF_FILES)
        message(FATAL_ERROR "Cannot attach empty modules to leaf component ${target_name}")
    endif()
    target_sources(${target_name}
        PUBLIC
        FILE_SET leaf_modules TYPE CXX_MODULES
        BASE_DIRS ${LEAF_BASE_DIRS}
        FILES ${LEAF_FILES})
endfunction()

# ExampleTemplate.cmake
#
# Usage:
#   include("${CMAKE_SOURCE_DIR}/Examples/cmake/ExampleTemplate.cmake")
#   charm_example_init(<target_name>)
#   charm_example_sources(<target_name> <base_dir> <globs...>)
#   charm_example_link_charm(<target_name> <charm_root>)
#   charm_example_sdl3_options()
#   charm_example_link_sdl3(<target_name> <charm_root>)

function(charm_example_init target_name)
    cmake_minimum_required(VERSION 4.0)
    project(${target_name})
    set(CMAKE_CXX_STANDARD 26)
endfunction()

function(charm_example_sources target_name base_dir)
    file(GLOB_RECURSE MODULE_INTERFACE_UNITS ${ARGN})
    target_sources(${target_name}
        PRIVATE
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS
            "${base_dir}"
        FILES
            ${MODULE_INTERFACE_UNITS}
    )
endfunction()

function(charm_example_sources_filtered target_name base_dir exclude_regex)
    file(GLOB_RECURSE MODULE_INTERFACE_UNITS ${ARGN})
    if (exclude_regex)
        list(FILTER MODULE_INTERFACE_UNITS EXCLUDE REGEX "${exclude_regex}")
    endif()
    target_sources(${target_name}
        PRIVATE
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS
            "${base_dir}"
        FILES
            ${MODULE_INTERFACE_UNITS}
    )
endfunction()

function(charm_example_link_charm target_name charm_root)
    add_subdirectory("${charm_root}" "${CMAKE_BINARY_DIR}/Charm")
    target_link_libraries(${target_name} PRIVATE Charm-os)
endfunction()

function(charm_example_sdl3_options)
    set(SDL_TEST OFF PARENT_SCOPE)
    set(SDL_VIDEO ON PARENT_SCOPE)
    set(SDL_AUDIO OFF PARENT_SCOPE)
    set(SDL_GPU ON PARENT_SCOPE)
    set(SDL_RENDER ON PARENT_SCOPE)
    set(SDL_CAMERA OFF PARENT_SCOPE)
    set(SDL_JOYSTICK OFF PARENT_SCOPE)
    set(SDL_HAPTIC OFF PARENT_SCOPE)
    set(SDL_POWER OFF PARENT_SCOPE)
    set(SDL_SENSOR OFF PARENT_SCOPE)
    set(SDL_DIALOG OFF PARENT_SCOPE)
    set(SDL_TRAY OFF PARENT_SCOPE)

    set(SDL_SHARED OFF PARENT_SCOPE)
    set(SDL_STATIC ON PARENT_SCOPE)
    set(SDL3_DISABLE_WINDOWS_MAIN ON PARENT_SCOPE)
endfunction()

function(charm_example_link_sdl3 target_name charm_root)
    include("${charm_root}/cmake/SDL3.cmake")
    charm_link_sdl3(${target_name})
endfunction()

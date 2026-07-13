cmake_minimum_required(VERSION 4.0)

if(TARGET Charm-host-sdl3)
    return()
endif()

option(CHARM_HOST_SDL3_TESTING "Enable Host SDL3 test-only output capture" OFF)

get_filename_component(CHARM_HOST_SDL3_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

if(NOT TARGET Charm::system)
    message(FATAL_ERROR "Charm-host-sdl3 requires Charm::system")
endif()

include("${CHARM_HOST_SDL3_ROOT}/cmake/SDL3.cmake")
charm_find_sdl3()

add_library(Charm-host-sdl3 STATIC)
add_library(Charm::host-sdl3 ALIAS Charm-host-sdl3)
add_library(Charm-host-sdl3-sdl-dependency INTERFACE)
target_link_libraries(Charm-host-sdl3-sdl-dependency INTERFACE SDL3::SDL3)

set_target_properties(Charm-host-sdl3 PROPERTIES
    CXX_STANDARD 26
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_sources(Charm-host-sdl3
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
    BASE_DIRS "${CHARM_HOST_SDL3_ROOT}"
    FILES "${CMAKE_CURRENT_LIST_DIR}/host_sdl3.cppm"
)

target_include_directories(Charm-host-sdl3 PUBLIC "${CHARM_HOST_SDL3_ROOT}")
target_link_libraries(Charm-host-sdl3
    PUBLIC Charm::system
    PRIVATE Charm-host-sdl3-sdl-dependency
)

if(CHARM_HOST_SDL3_TESTING)
    target_compile_definitions(Charm-host-sdl3 PRIVATE CHARM_HOST_SDL3_TESTING=1)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(Charm-host-sdl3 PRIVATE -Wall -Wextra -Wpedantic)
endif()

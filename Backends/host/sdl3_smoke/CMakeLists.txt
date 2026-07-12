cmake_minimum_required(VERSION 4.0)
project(backends-host-sdl3-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

get_filename_component(CHARM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

set(CHARM_BUILD_FULL_RUNTIME ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_BUILD_PLAYER OFF CACHE BOOL "" FORCE)
set(CHARM_BUILD_TARGET_BOOTSTRAP OFF CACHE BOOL "" FORCE)
set(CHARM_HOST_SDL3_TESTING ON CACHE BOOL "" FORCE)

set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_VIDEO ON CACHE BOOL "" FORCE)
set(SDL_RENDER ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL3_DISABLE_WINDOWS_MAIN ON CACHE BOOL "" FORCE)

add_subdirectory("${CHARM_ROOT}" charm-root EXCLUDE_FROM_ALL)
add_subdirectory("${CHARM_ROOT}/Backends/host/sdl3" charm-host-sdl3)

add_library(backends-host-sdl3-smoke-fixture OBJECT
    sdl_event_fixture.cpp
)
target_link_libraries(backends-host-sdl3-smoke-fixture PRIVATE
    Charm-host-sdl3-sdl-dependency
)

add_executable(${target_name}
    main.cpp
    $<TARGET_OBJECTS:backends-host-sdl3-smoke-fixture>
)

target_include_directories(${target_name} PRIVATE "${CHARM_ROOT}")
target_link_libraries(${target_name} PRIVATE Charm::host-sdl3)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME backends_host_sdl3_smoke COMMAND ${target_name})
set_tests_properties(backends_host_sdl3_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[backends-host-sdl3-smoke\\] ok")

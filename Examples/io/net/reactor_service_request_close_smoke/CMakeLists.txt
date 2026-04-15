cmake_minimum_required(VERSION 4.0)
project(net-reactor-service-request-close-smoke)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

if (NOT WIN32)
    message(FATAL_ERROR "net-reactor-service-request-close-smoke currently requires Windows")
endif()

add_executable(${target_name}
    main.cpp
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)

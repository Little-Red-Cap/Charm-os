cmake_minimum_required(VERSION 4.0)
project(usb-cdc-replay-smoke)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
    main.cpp
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)

cmake_path(CONVERT "${CMAKE_CURRENT_SOURCE_DIR}/cdc.trace" TO_CMAKE_PATH_LIST usb_replay_fixture_path)
target_compile_definitions(${target_name} PRIVATE USB_REPLAY_FIXTURE_PATH="${usb_replay_fixture_path}")

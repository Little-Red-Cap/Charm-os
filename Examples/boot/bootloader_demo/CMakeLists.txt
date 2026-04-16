cmake_minimum_required(VERSION 4.0)
project(bootloader-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(${target_name}
    main.cpp
    "${CHARM_ROOT}/targets/armv7a/common/armv7a_handoff_contract.cpp"
)

set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_include_directories(${target_name} PRIVATE
    "${CHARM_ROOT}/targets/armv7a/common")
target_link_libraries(${target_name} PRIVATE Charm-os)

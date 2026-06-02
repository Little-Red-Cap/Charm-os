cmake_minimum_required(VERSION 4.0)
project(resident-platform-inspect-tool LANGUAGES CXX)
set(target_name resident-platform-inspect)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(${target_name} main.cpp)
target_include_directories(${target_name} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CHARM_ROOT}/Examples/app_abi"
    "${CHARM_ROOT}/Examples/dev_loader")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

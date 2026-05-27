cmake_minimum_required(VERSION 4.0)
project(app-abi-store-install-smoke LANGUAGES CXX)
# Avoid Windows installer/UAC heuristics on executables containing "install".
set(target_name app-abi-store-write-smoke)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(${target_name} main.cpp)
target_include_directories(${target_name} PRIVATE
    "${CHARM_ROOT}/Examples/app_abi")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME app_abi_store_install_smoke COMMAND ${target_name})
set_tests_properties(app_abi_store_install_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[app-abi-store-install-smoke\\] ok")

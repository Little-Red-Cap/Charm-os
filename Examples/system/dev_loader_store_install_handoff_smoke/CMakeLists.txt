cmake_minimum_required(VERSION 4.0)
project(dev-loader-store-install-handoff-smoke LANGUAGES CXX)
set(target_name dev-loader-store-handoff-smoke)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(${target_name} main.cpp)
target_include_directories(${target_name} PRIVATE
    "${CHARM_ROOT}/Examples/app_abi"
    "${CHARM_ROOT}/Examples/dev_loader")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME dev_loader_store_install_handoff_smoke COMMAND ${target_name})
set_tests_properties(dev_loader_store_install_handoff_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[dev-loader-store-install-handoff-smoke\\] ok")

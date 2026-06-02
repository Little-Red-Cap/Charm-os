cmake_minimum_required(VERSION 4.0)
project(resident-platform-artifact-smoke LANGUAGES CXX)
set(target_name resident-platform-artifact-smoke)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(${target_name} main.cpp)
target_include_directories(${target_name} PRIVATE
    "${CHARM_ROOT}/Examples/app_abi"
    "${CHARM_ROOT}/Examples/dev_loader")
target_compile_definitions(${target_name} PRIVATE
    CHARM_ARTIFACT_OUT_DIR="${CHARM_ROOT}/Examples/app_abi/elf_samples/out")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME resident_platform_artifact_smoke COMMAND ${target_name})
set_tests_properties(resident_platform_artifact_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[resident-platform-artifact-smoke\\] ok")

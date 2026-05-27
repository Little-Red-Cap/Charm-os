cmake_minimum_required(VERSION 4.0)
project(dev-loader-stage-probe-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(${target_name} main.cpp)
target_include_directories(${target_name} PRIVATE
    "${CHARM_ROOT}/Examples/app_abi"
    "${CHARM_ROOT}/Examples/dev_loader")

target_compile_definitions(${target_name} PRIVATE
    CHARM_APP_ABI_ELF_SAMPLE_DIR="${CHARM_ROOT}/Examples/app_abi/elf_samples/out")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME dev_loader_stage_probe_smoke COMMAND ${target_name})
set_tests_properties(dev_loader_stage_probe_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[dev-loader-stage-probe-smoke\\] ok")

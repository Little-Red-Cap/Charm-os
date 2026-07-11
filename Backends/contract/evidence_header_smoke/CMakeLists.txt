cmake_minimum_required(VERSION 4.0)
project(backends-contract-evidence-header-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_executable(${target_name}
    main.cpp
)

target_include_directories(${target_name} PRIVATE ../../..)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME backends_contract_evidence_header_smoke COMMAND ${target_name})
set_tests_properties(backends_contract_evidence_header_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[backends-contract-evidence-header-smoke\\] ok")

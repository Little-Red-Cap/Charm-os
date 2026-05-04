cmake_minimum_required(VERSION 4.0)
project(vivid-evidence-lab-manifest-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
enable_testing()

add_executable(${target_name}
    main.cpp
)
target_compile_definitions(${target_name} PRIVATE
    "CHARM_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/../../../..\"")

add_test(NAME ${target_name}
    COMMAND $<TARGET_FILE:${target_name}>)
set_tests_properties(${target_name} PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[elm\\] run=evidence_lab_manifest_demo phase=end result=ok cases=8"
    FAIL_REGULAR_EXPRESSION "\\[ERR\\]|result=fail")

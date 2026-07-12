cmake_minimum_required(VERSION 4.0)
project(charm-capability-mvp-host LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${target_name}
    main.cpp
)

set(matrix_target_name charm-capability-mvp-host-failure-matrix)
add_executable(${matrix_target_name}
    host_failure_matrix.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(${matrix_target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME charm_capability_mvp_host COMMAND ${target_name})
set_tests_properties(charm_capability_mvp_host PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[charm-capability-mvp-host\\] ok")
add_test(NAME charm_capability_mvp_host_failure_matrix COMMAND ${matrix_target_name})
set_tests_properties(charm_capability_mvp_host_failure_matrix PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[charm-capability-mvp-host-matrix\\] ok")

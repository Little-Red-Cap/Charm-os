cmake_minimum_required(VERSION 4.0)
project(rte-profile-selection-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

add_executable(${target_name}
    main.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME rte_profile_selection_smoke COMMAND ${target_name})
set_tests_properties(rte_profile_selection_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[rte-profile-selection-smoke\\] ok")

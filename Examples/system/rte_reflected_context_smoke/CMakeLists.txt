cmake_minimum_required(VERSION 4.0)
project(rte-reflected-context-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${target_name}
    main.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -freflection)
endif()

enable_testing()
add_test(NAME rte_reflected_context_smoke COMMAND ${target_name})
set_tests_properties(rte_reflected_context_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[rte-reflected-context-smoke\\] ok")

cmake_minimum_required(VERSION 4.0)
project(charm-capability-relations LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(CHARM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/charm-core" EXCLUDE_FROM_ALL)

foreach(test_name IN ITEMS model resolution)
    set(target_name "charm-capability-relations-${test_name}")
    add_executable(${target_name} "${test_name}.cpp")
    target_link_libraries(${target_name} PRIVATE Charm::core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

foreach(case_name IN ITEMS
        same_requirement_key
        same_provision_key
        same_binding_key
        default_construction)
    set(target_name "charm-capability-relations-negative-${case_name}")
    add_library(${target_name} OBJECT EXCLUDE_FROM_ALL
        "negative_${case_name}.cpp")
    target_link_libraries(${target_name} PRIVATE Charm::core)
endforeach()

set(projection_target charm-capability-relations-projection)
add_executable(${projection_target} projection.cpp)
target_link_libraries(${projection_target} PRIVATE Charm::core)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${projection_target} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME charm_capability_relations_model
         COMMAND charm-capability-relations-model)
set_tests_properties(charm_capability_relations_model PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[charm-capability-relations-model\\] ok")
add_test(NAME charm_capability_relations_resolution
         COMMAND charm-capability-relations-resolution)
set_tests_properties(charm_capability_relations_resolution PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[charm-capability-relations-resolution\\] ok")
add_test(NAME charm_capability_relations_projection
         COMMAND charm-capability-relations-projection)
set_tests_properties(charm_capability_relations_projection PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[charm-capability-relations-projection\\] ok")

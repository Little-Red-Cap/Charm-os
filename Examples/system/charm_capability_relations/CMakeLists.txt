cmake_minimum_required(VERSION 4.0)
project(charm-capability-relations LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

get_filename_component(CHARM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

foreach(test_name IN ITEMS model resolution)
    set(target_name "charm-capability-relations-${test_name}")
    add_executable(${target_name} "${test_name}.cpp")
    target_include_directories(${target_name} PRIVATE "${CHARM_ROOT}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()

set(projection_target charm-capability-relations-projection)
add_executable(${projection_target} projection.cpp)
target_include_directories(${projection_target} PRIVATE "${CHARM_ROOT}")
target_sources(${projection_target}
    PRIVATE
    FILE_SET charm_modules TYPE CXX_MODULES
    BASE_DIRS "${CHARM_ROOT}/Modules"
    FILES
        "${CHARM_ROOT}/Modules/core/util/core.cppm"
        "${CHARM_ROOT}/Modules/core/util/expected.cppm"
        "${CHARM_ROOT}/Modules/core/util/error.cppm"
        "${CHARM_ROOT}/Modules/core/init/init.node.cppm"
        "${CHARM_ROOT}/Modules/core/init/init.graph.cppm")
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

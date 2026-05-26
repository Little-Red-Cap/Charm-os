cmake_minimum_required(VERSION 4.0)
project(rte-profile-materialization-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(${target_name}
    main.cpp
)

target_sources(${target_name}
    PRIVATE
    FILE_SET charm_modules TYPE CXX_MODULES
    BASE_DIRS "${CHARM_ROOT}/Modules"
    FILES
        "${CHARM_ROOT}/Modules/core/util/core.cppm"
        "${CHARM_ROOT}/Modules/core/util/expected.cppm"
        "${CHARM_ROOT}/Modules/core/util/error.cppm"
        "${CHARM_ROOT}/Modules/core/init/init.node.cppm"
        "${CHARM_ROOT}/Modules/core/init/init.graph.cppm"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME rte_profile_materialization_smoke COMMAND ${target_name})
set_tests_properties(rte_profile_materialization_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[rte-profile-materialization-smoke\\] ok")

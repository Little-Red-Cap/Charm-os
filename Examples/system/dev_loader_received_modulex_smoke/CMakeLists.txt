cmake_minimum_required(VERSION 4.0)
project(dev-loader-received-modulex-smoke LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(${target_name}
    main.cpp)

target_include_directories(${target_name} PRIVATE
    "${CHARM_ROOT}/Examples/app_abi"
    "${CHARM_ROOT}/Examples/dev_loader")

target_sources(${target_name}
    PRIVATE
    FILE_SET charm_modules TYPE CXX_MODULES
    BASE_DIRS "${CHARM_ROOT}/Modules"
    FILES
        "${CHARM_ROOT}/Modules/core/util/core.cppm"
        "${CHARM_ROOT}/Modules/system/modulex/module_core.cppm"
        "${CHARM_ROOT}/Modules/system/modulex/module_view.cppm"
        "${CHARM_ROOT}/Modules/system/modulex/module_loader.cppm"
        "${CHARM_ROOT}/Modules/system/modulex/module_link.cppm")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME dev_loader_received_modulex_smoke COMMAND ${target_name})
set_tests_properties(dev_loader_received_modulex_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[dev-loader-received-modulex-smoke\\] ok")

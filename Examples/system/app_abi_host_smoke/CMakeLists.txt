cmake_minimum_required(VERSION 4.0)
project(app-abi-host-smoke LANGUAGES C CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

add_executable(${target_name}
    main.cpp
    "${CHARM_ROOT}/Examples/app_abi/elf_samples/hello_app.c"
    "${CHARM_ROOT}/Examples/app_abi/elf_samples/player_min.c")

target_include_directories(${target_name} PRIVATE
    "${CHARM_ROOT}/Examples/app_abi")

set_source_files_properties(
    "${CHARM_ROOT}/Examples/app_abi/elf_samples/hello_app.c"
    PROPERTIES COMPILE_DEFINITIONS charm_app_main=hello_app_main)
set_source_files_properties(
    "${CHARM_ROOT}/Examples/app_abi/elf_samples/player_min.c"
    PROPERTIES COMPILE_DEFINITIONS charm_app_main=player_min_main)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME app_abi_host_smoke COMMAND ${target_name})
set_tests_properties(app_abi_host_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[app-abi-host-smoke\\] ok")

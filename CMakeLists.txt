cmake_minimum_required(VERSION 4.0)
project(Charm-os LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-module-lazy>)
endif()

option(CHARM_ENABLE_TEST_MODULES "Include Modules/*/tests/*.cppm" OFF)
option(CHARM_TARGET_HAS_HOSTED_CXX "Target provides hosted C++ facilities" ON)
option(CHARM_TARGET_HAS_CXX_MATH "Target provides C++ math facilities" ON)

add_library(Charm-build-config INTERFACE)
add_library(Charm::build-config ALIAS Charm-build-config)
target_compile_options(Charm-build-config INTERFACE -Wall)
target_compile_definitions(Charm-build-config INTERFACE
    CHARM_LIB_BUILD=1
    CHARM_TARGET_HAS_HOSTED_CXX=$<BOOL:${CHARM_TARGET_HAS_HOSTED_CXX}>
    CHARM_TARGET_HAS_CXX_MATH=$<BOOL:${CHARM_TARGET_HAS_CXX_MATH}>)

add_library(Charm-core INTERFACE)
target_link_libraries(Charm-core INTERFACE Charm-build-config)
target_include_directories(Charm-core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules")
target_sources(Charm-core INTERFACE
    FILE_SET headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Modules"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/Modules/core/capability/relations.hpp")
add_library(Charm::core ALIAS Charm-core)

add_library(Charm-os INTERFACE)
target_link_libraries(Charm-os INTERFACE Charm-core)
add_library(Charm::os ALIAS Charm-os)

message(STATUS "OnlyCore: exposing the admitted Core relation header")

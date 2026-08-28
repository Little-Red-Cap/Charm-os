cmake_minimum_required(VERSION 4.0)
project(CharmCore LANGUAGES CXX)

add_library(Charm-core INTERFACE)
target_compile_features(Charm-core INTERFACE cxx_std_26)
target_include_directories(Charm-core INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/Modules")
target_sources(Charm-core INTERFACE
    FILE_SET headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Modules"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/Modules/core/capability/relations.hpp")
add_library(Charm::core ALIAS Charm-core)

message(STATUS "OnlyCore: Charm::core exposes the admitted relation header")

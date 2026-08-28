cmake_minimum_required(VERSION 4.0)
project(CharmCore LANGUAGES CXX)

include(GNUInstallDirs)

add_library(Charm-core INTERFACE)
target_compile_features(Charm-core INTERFACE cxx_std_26)
target_include_directories(Charm-core INTERFACE
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Modules>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_sources(Charm-core INTERFACE
    FILE_SET headers TYPE HEADERS
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/Modules"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/Modules/core/capability/relations.hpp")
set_target_properties(Charm-core PROPERTIES EXPORT_NAME core)
add_library(Charm::core ALIAS Charm-core)

install(TARGETS Charm-core
    EXPORT CharmCoreTargets
    FILE_SET headers DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(EXPORT CharmCoreTargets
    FILE CharmCoreConfig.cmake
    NAMESPACE Charm::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/CharmCore")

message(STATUS "OnlyCore: Charm::core exposes and installs the admitted relation header")

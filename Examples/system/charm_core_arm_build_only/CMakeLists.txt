cmake_minimum_required(VERSION 4.0)
project(charm-core-arm-build-only LANGUAGES CXX)

get_filename_component(CHARM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/charm-core" EXCLUDE_FROM_ALL)

add_library(charm-core-arm-build-only OBJECT smoke.cpp)
target_link_libraries(charm-core-arm-build-only PRIVATE Charm::core)
target_compile_options(charm-core-arm-build-only PRIVATE
    -ffreestanding
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -Wall
    -Wextra
    -Wpedantic
    -Werror)
set_target_properties(charm-core-arm-build-only PROPERTIES
    CXX_EXTENSIONS OFF)

cmake_minimum_required(VERSION 3.28)
project(app-abi-modulex-pack-tool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(CHARM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

add_executable(app-abi-modulex-pack main.cpp)
target_include_directories(app-abi-modulex-pack PRIVATE
    "${CHARM_ROOT}/Modules"
    "${CHARM_ROOT}/Examples/app_abi")

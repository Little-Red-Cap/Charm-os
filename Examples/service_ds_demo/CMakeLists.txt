cmake_minimum_required(VERSION 4.0)
project(service-ds-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
    main.cpp
)

file(GLOB_RECURSE MODULE_INTERFACE_UNITS
    "../../Modules/*.cppm"
)

target_sources(${target_name}
    PRIVATE
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS
        "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    FILES
        ${MODULE_INTERFACE_UNITS}
)

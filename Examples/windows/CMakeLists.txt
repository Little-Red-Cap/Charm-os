cmake_minimum_required(VERSION 4.0)
project(os-example-win)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
#        main.cpp
        main_m1.cpp
#        main_m2.cpp
#        main_m3.cpp
        port.kernel.windows.cpp
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

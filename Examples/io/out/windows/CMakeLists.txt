cmake_minimum_required(VERSION 4.0)
project(out-example-win)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)


add_executable(${target_name}
        main.cpp
        out.port.windows.cpp
        ../example.cpp
)

# Usage 1:  build source
file(GLOB_RECURSE MODULE_INTERFACE_UNITS "../../modules/*.cppm")
target_sources(${target_name}
        PUBLIC
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS
            "${CMAKE_CURRENT_SOURCE_DIR}/../../"
        FILES
            ${MODULE_INTERFACE_UNITS}
)

target_compile_definitions(${target_name}
        PRIVATE
        LOG_LEVEL_DEBUG
        OUT_ENABLE_BINARY
        OUT_ENABLE_FLOAT
)

# Usage 2:  link lib
#target_link_libraries(${target_name} PRIVATE out_lib)

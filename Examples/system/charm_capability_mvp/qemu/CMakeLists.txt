cmake_minimum_required(VERSION 4.0)
project(charm-capability-mvp-qemu LANGUAGES CXX)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(${target_name}
    startup.cpp
    syscalls.cpp
    main.cpp
)

target_include_directories(${target_name} PRIVATE ..)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
endif()

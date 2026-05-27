cmake_minimum_required(VERSION 4.0)
project(static_reflection_probe LANGUAGES NONE)

set(STATIC_REFLECTION_GXX "" CACHE FILEPATH "Path to an arm-none-eabi-g++ with __cpp_impl_reflection support")
if(NOT STATIC_REFLECTION_GXX)
    find_program(STATIC_REFLECTION_GXX
        NAMES arm-none-eabi-g++.exe arm-none-eabi-g++
        DOC "arm-none-eabi g++ for the static reflection probe")
endif()

if(NOT STATIC_REFLECTION_GXX)
    message(FATAL_ERROR "Could not find arm-none-eabi-g++ for the static reflection probe.")
endif()

enable_testing()
add_test(NAME static_reflection_probe
    COMMAND "${CMAKE_COMMAND}"
        "-DSTATIC_REFLECTION_GXX=${STATIC_REFLECTION_GXX}"
        "-DSTATIC_REFLECTION_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/probe.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_probe.cmake")

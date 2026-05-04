cmake_minimum_required(VERSION 4.0)
project(vivid-semantic-default-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)
enable_testing()

add_executable(${target_name}
    main.cpp
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_FREETYPE ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID ON CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)

add_test(NAME ${target_name}
    COMMAND $<TARGET_FILE:${target_name}>)
set_tests_properties(${target_name} PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[sdef\\] run=semantic_default_demo phase=end result=ok cases=6"
    FAIL_REGULAR_EXPRESSION "\\[ERR\\]|result=fail")

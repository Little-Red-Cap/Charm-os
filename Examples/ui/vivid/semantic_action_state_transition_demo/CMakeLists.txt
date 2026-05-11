cmake_minimum_required(VERSION 4.0)
project(vivid-semantic-action-state-transition-demo)
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
set(CHARM_VIVID_SCREEN_WIDTH 32 CACHE STRING "" FORCE)
set(CHARM_VIVID_SCREEN_HEIGHT 32 CACHE STRING "" FORCE)
set(CHARM_VIVID_LAYER_CACHE_SLOTS 2 CACHE STRING "" FORCE)
set(CHARM_VIVID_LAYER_CACHE_WIDTH 32 CACHE STRING "" FORCE)
set(CHARM_VIVID_LAYER_CACHE_HEIGHT 32 CACHE STRING "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)

add_test(NAME ${target_name}
    COMMAND $<TARGET_FILE:${target_name}>)
set_tests_properties(${target_name} PROPERTIES
    PASS_REGULAR_EXPRESSION "\\[sastx\\] run=semantic_action_state_transition_demo phase=end result=ok cases=10"
    FAIL_REGULAR_EXPRESSION "\\[ERR\\]|result=fail")

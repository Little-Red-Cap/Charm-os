cmake_minimum_required(VERSION 4.0)
project(vivid-page-transition-demo)
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
if(CHARM_VIVID_SNAPSHOT_STORAGE_MODE STREQUAL "COMMAND")
    set(_vivid_page_transition_cache_slots 1)
else()
    set(_vivid_page_transition_cache_slots 2)
endif()
set(CHARM_VIVID_LAYER_CACHE_SLOTS
    ${_vivid_page_transition_cache_slots} CACHE STRING "" FORCE)
set(CHARM_VIVID_LAYER_CACHE_WIDTH 160 CACHE STRING "" FORCE)
set(CHARM_VIVID_LAYER_CACHE_HEIGHT 96 CACHE STRING "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)

add_test(NAME ${target_name}
    COMMAND $<TARGET_FILE:${target_name}>)
if(CHARM_VIVID_SNAPSHOT_STORAGE_MODE STREQUAL "COMMAND")
    set(_vivid_page_transition_pass_gate
        "\\[pt\\] run=page_transition_demo phase=end result=ok cases=5")
elseif(CHARM_VIVID_SNAPSHOT_STORAGE_MODE STREQUAL "PIXEL")
    set(_vivid_page_transition_pass_gate
        "\\[pt\\] run=page_transition_demo phase=end result=ok cases=16")
else()
    set(_vivid_page_transition_pass_gate
        "\\[pt\\] run=page_transition_demo phase=end result=ok cases=18")
endif()
set_tests_properties(${target_name} PROPERTIES
    PASS_REGULAR_EXPRESSION "${_vivid_page_transition_pass_gate}"
    FAIL_REGULAR_EXPRESSION "\\[ERR\\]|result=fail")

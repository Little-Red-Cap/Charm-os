cmake_minimum_required(VERSION 4.0)
project(init-bringup-block-observe-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
    main.cpp
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)

set(BRINGUP_BLOCK_OBSERVE_DOT_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/bringup_block_materialized_graph.dot"
    CACHE FILEPATH "DOT export path for bringup block observe demo")
set(BRINGUP_BLOCK_OBSERVE_JSON_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/bringup_block_materialized_graph.sample.json"
    CACHE FILEPATH "JSON export path for bringup block observe demo")
set(BRINGUP_BLOCK_OBSERVE_IMAGE_PATH
    "observe-block.img"
    CACHE STRING "Placeholder image path used by bringup block observe demo")

add_custom_target(export_bringup_block_materialized_graph
    COMMAND $<TARGET_FILE:${target_name}>
        --dot "${BRINGUP_BLOCK_OBSERVE_DOT_PATH}"
        --json "${BRINGUP_BLOCK_OBSERVE_JSON_PATH}"
        --image "${BRINGUP_BLOCK_OBSERVE_IMAGE_PATH}"
    DEPENDS ${target_name}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM)

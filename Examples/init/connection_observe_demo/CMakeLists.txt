cmake_minimum_required(VERSION 4.0)
project(init-connection-observe-demo)
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

set(CONNECTION_OBSERVE_DOT_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/connection_graph.dot"
    CACHE FILEPATH "DOT export path for connection observe demo")
set(CONNECTION_OBSERVE_JSON_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/connection_graph.sample.json"
    CACHE FILEPATH "JSON export path for connection observe demo")

add_custom_target(export_connection_graph_demo
    COMMAND $<TARGET_FILE:${target_name}>
        --dot "${CONNECTION_OBSERVE_DOT_PATH}"
        --json "${CONNECTION_OBSERVE_JSON_PATH}"
    DEPENDS ${target_name}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM)

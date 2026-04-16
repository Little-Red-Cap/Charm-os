cmake_minimum_required(VERSION 4.0)
project(usb-msc-block-demo)
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

set(USB_MSC_BLOCK_EXPORT_IMAGE_PATH
    "observe-usb-block.img"
    CACHE STRING "Placeholder image path used by usb_msc_block_demo export-only mode")
set(USB_MSC_BLOCK_EXPORT_DOT_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/usb_msc_block_materialized_graph.dot"
    CACHE FILEPATH "DOT export path for usb_msc_block_demo")
set(USB_MSC_BLOCK_EXPORT_JSON_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/usb_msc_block_materialized_graph.sample.json"
    CACHE FILEPATH "JSON export path for usb_msc_block_demo")
set(USB_MSC_BLOCK_RUNTIME_OBSERVE_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/usb_msc_block.runtime_observe.json"
    CACHE FILEPATH "Runtime observe snapshot path for usb_msc_block_demo")
set(USB_MSC_BLOCK_RUNTIME_IMAGE_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/observe-usb-block.runtime.img"
    CACHE FILEPATH "Block image path used by usb_msc_block_demo runtime observe export")

add_custom_target(export_usb_msc_block_materialized_graph
    COMMAND $<TARGET_FILE:${target_name}>
        --export-only
        --dot "${USB_MSC_BLOCK_EXPORT_DOT_PATH}"
        --json "${USB_MSC_BLOCK_EXPORT_JSON_PATH}"
        --image "${USB_MSC_BLOCK_EXPORT_IMAGE_PATH}"
    DEPENDS ${target_name}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM)

add_custom_target(export_usb_msc_block_runtime_observe
    COMMAND $<TARGET_FILE:${target_name}>
        --runtime-observe "${USB_MSC_BLOCK_RUNTIME_OBSERVE_PATH}"
        --image "${USB_MSC_BLOCK_RUNTIME_IMAGE_PATH}"
    DEPENDS ${target_name}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM)

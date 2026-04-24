cmake_minimum_required(VERSION 4.0)
project(usb-host-runtime-multi-smoke)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
    main.cpp
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_POSIX OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)

set(USB_HOST_RUNTIME_MULTI_RUNTIME_OBSERVE_PATH
    "${CMAKE_CURRENT_BINARY_DIR}/usb_host_runtime_multi.runtime_observe.json"
    CACHE FILEPATH "Runtime observe snapshot path for usb_host_runtime_multi_smoke")

add_custom_target(export_usb_host_runtime_multi_runtime_observe
    COMMAND $<TARGET_FILE:${target_name}>
        --runtime-observe "${USB_HOST_RUNTIME_MULTI_RUNTIME_OBSERVE_PATH}"
    DEPENDS ${target_name}
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    VERBATIM)

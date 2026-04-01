cmake_minimum_required(VERSION 4.0)
project(rtos-qemu-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
    startup.cpp
    main.cpp
)

target_compile_definitions(${target_name} PRIVATE
    CHARM_KERNEL_REQUIRE_SSU_META=1
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_compile_definitions(Charm-os PRIVATE
    CHARM_KERNEL_REQUIRE_SSU_META=1
)
target_link_libraries(${target_name} PRIVATE Charm-os)

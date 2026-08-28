cmake_minimum_required(VERSION 4.0)
project(charm-capability-mvp-qemu LANGUAGES ASM CXX)

get_filename_component(CHARM_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/charm-core" EXCLUDE_FROM_ALL)

add_executable(charm-capability-mvp-qemu
    main.cpp
    qemu_platform.cpp
    startup.S)
target_link_libraries(charm-capability-mvp-qemu PRIVATE
    Charm::core
    gcc)
target_include_directories(charm-capability-mvp-qemu PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/..")
target_compile_options(charm-capability-mvp-qemu PRIVATE
    -mcpu=cortex-a15
    -marm
    -mno-unaligned-access
    -O2
    -ffreestanding
    -fno-builtin
    -fno-exceptions
    -fno-rtti
    -fno-threadsafe-statics
    -fno-use-cxa-atexit
    -fno-unwind-tables
    -fno-asynchronous-unwind-tables
    -fdata-sections
    -ffunction-sections
    -Wall
    -Wextra
    -Wpedantic
    -Werror)
target_link_options(charm-capability-mvp-qemu PRIVATE
    -mcpu=cortex-a15
    -marm
    -mno-unaligned-access
    -O2
    -nostdlib
    "-Wl,-T,${CMAKE_CURRENT_LIST_DIR}/linker.ld"
    -Wl,--gc-sections
    -Wl,--build-id=none)
set_target_properties(charm-capability-mvp-qemu PROPERTIES
    CXX_EXTENSIONS OFF
    LINK_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/linker.ld"
    SUFFIX ".elf")

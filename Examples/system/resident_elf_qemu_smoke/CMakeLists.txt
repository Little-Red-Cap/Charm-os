cmake_minimum_required(VERSION 4.0)
project(resident-elf-qemu-smoke LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(CHARM_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
    ABSOLUTE)

if(NOT DEFINED CHARM_QEMU_APP_ELF_INC_DIR)
    message(FATAL_ERROR "CHARM_QEMU_APP_ELF_INC_DIR must point at generated QEMU App ELF .inc files")
endif()

set(CHARM_QEMU_REQUIRED_INC_FILES
    appstore.bin.inc
    argv_app.elf.inc
    bss_app.elf.inc
    console_error_app.elf.inc
    data_app.elf.inc
    display_describe_error_app.elf.inc
    display_error_app.elf.inc
    display_sequence_app.elf.inc
    exit_app.elf.inc
    exit_error_app.elf.inc
    hello_app.elf.inc
    input_error_app.elf.inc
    input_sequence_app.elf.inc
    input_wrap_app.elf.inc
    large_fit_app.elf.inc
    player_min.elf.inc
    storage_app.elf.inc
    storage_catalog_app.elf.inc
    storage_close_error_app.elf.inc
    storage_error_app.elf.inc
    storage_fd_exhaustion_app.elf.inc
    storage_open_error_app.elf.inc
    storage_write_error_app.elf.inc
    time_app.elf.inc
    time_sequence_app.elf.inc
    too_large_app.elf.inc
    unsupported_caps_app.elf.inc)

set(CHARM_QEMU_MISSING_INC_FILES)
foreach(required_inc IN LISTS CHARM_QEMU_REQUIRED_INC_FILES)
    if(NOT EXISTS "${CHARM_QEMU_APP_ELF_INC_DIR}/${required_inc}")
        list(APPEND CHARM_QEMU_MISSING_INC_FILES "${required_inc}")
    endif()
endforeach()

if(CHARM_QEMU_MISSING_INC_FILES)
    list(JOIN CHARM_QEMU_MISSING_INC_FILES ", " CHARM_QEMU_MISSING_INC_FILES_TEXT)
    message(FATAL_ERROR
        "missing generated QEMU resident ELF artifacts in ${CHARM_QEMU_APP_ELF_INC_DIR}: "
        "${CHARM_QEMU_MISSING_INC_FILES_TEXT}. "
        "Run Examples/system/run-resident-elf-qemu-smoke.ps1 to regenerate them.")
endif()

add_executable(resident-elf-qemu-smoke
    startup.cpp
    syscalls.cpp
    qemu_virtual_backend.cpp
    main.cpp)

target_include_directories(resident-elf-qemu-smoke PRIVATE
    "${CHARM_ROOT}/Examples/app_abi"
    "${CHARM_ROOT}/Examples/dev_loader"
    "${CHARM_QEMU_APP_ELF_INC_DIR}")

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(resident-elf-qemu-smoke PRIVATE
        -Wall
        -Wextra
        -Wpedantic)
endif()

cmake_minimum_required(VERSION 4.0)
project(posix-qemu-demo)
set(target_name ${PROJECT_NAME})

set(CMAKE_CXX_STANDARD 26)

add_executable(${target_name}
    startup.cpp
    main.cpp
)

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../..")
set(CHARM_ENABLE_MEDIA OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_INK OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_UI_VIVID OFF CACHE BOOL "" FORCE)
set(CHARM_ENABLE_SDL3 OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHARM_ROOT}" "${CMAKE_BINARY_DIR}/Charm")
target_link_libraries(${target_name} PRIVATE Charm-os)
target_compile_definitions(Charm-os PRIVATE POSIX_TEST_BUILD=1)
target_compile_definitions(${target_name} PRIVATE
    POSIX_SMOKE_USE_UART=1
    POSIX_TEST_BUILD=1
    POSIX_ERRNO_SMOKE_TEST=1
    POSIX_FD_TABLE_SMOKE_TEST=1
    POSIX_PIPE_SMOKE_TEST=1
    POSIX_PROC_SMOKE_TEST=1
    POSIX_SJLJ_SMOKE_TEST=1
    POSIX_PROGRAMS_SMOKE_TEST=1
    POSIX_API_SMOKE_TEST=1
)

target_sources(${target_name}
    PRIVATE
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS
        "${CHARM_ROOT}"
    FILES
        "${CHARM_ROOT}/Examples/posix/tests/posix.api.tests.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.errno.tests.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.fd_table.tests.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.pipe.tests.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.proc.tests.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.sjlj.tests.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.test_harness.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.programs.exec.tests.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.programs.fdpath.tests.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.programs.shell.tests.cppm"
        "${CHARM_ROOT}/Examples/posix/tests/posix.programs.tests.cppm"
)

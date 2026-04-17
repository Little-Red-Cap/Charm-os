cmake_minimum_required(VERSION 4.0)
project(kernel-runtime-task-api-host LANGUAGES CXX)

set(target_name ${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
enable_testing()

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

set(kernel_runtime_task_api_modules
    "${CHARM_ROOT}/Modules/core/util/core.cppm"
    "${CHARM_ROOT}/Modules/core/util/type_list.cppm"
    "${CHARM_ROOT}/Modules/core/util/variant.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/context.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/eda.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/evt.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/runtime_bridge.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/runtime_glue.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/runtime_service.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/runtime_trap.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/ssu.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_runtime_api.cppm"
)

add_executable(${target_name}
    main.cpp
)

target_sources(${target_name}
    PRIVATE
        FILE_SET modules TYPE CXX_MODULES
        BASE_DIRS
            "${CHARM_ROOT}/Modules"
        FILES
            ${kernel_runtime_task_api_modules}
)

add_test(NAME ${target_name}
    COMMAND $<TARGET_FILE:${target_name}>)

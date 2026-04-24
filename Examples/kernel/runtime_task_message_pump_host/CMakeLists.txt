cmake_minimum_required(VERSION 4.0)
project(kernel-runtime-task-message-pump-host LANGUAGES CXX)

set(target_name ${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
enable_testing()

set(CHARM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")

set(kernel_runtime_demo_modules
    "${CHARM_ROOT}/Modules/core/util/core.cppm"
    "${CHARM_ROOT}/Modules/core/util/error.cppm"
    "${CHARM_ROOT}/Modules/core/util/expected.cppm"
    "${CHARM_ROOT}/Modules/core/util/type_list.cppm"
    "${CHARM_ROOT}/Modules/core/util/variant.cppm"
    "${CHARM_ROOT}/Modules/core/trace_core.cppm"
    "${CHARM_ROOT}/Modules/io/out/out.core.cppm"
    "${CHARM_ROOT}/Modules/io/out/out.sink.cppm"
    "${CHARM_ROOT}/Modules/io/out/out.format.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/capabilities.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/config.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/context.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/eda.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/evt.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/event_queue.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/event_queue_list.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/event_token.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/runtime_bridge.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/runtime_glue.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/runtime_mailbox.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/scheduler.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/ssu.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_message_api.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_message_dispatch.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_message_service_drain.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_message_service_loop.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_message_service_pump.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_message_table.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_state.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/thread.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/timer.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/trace.cppm"
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
            ${kernel_runtime_demo_modules}
)

target_include_directories(${target_name} PRIVATE
    "${CHARM_ROOT}/Modules/io/out"
)

target_compile_definitions(${target_name} PRIVATE
    CHARM_KERNEL_REQUIRE_SSU_META=1
)

add_test(NAME ${target_name}
    COMMAND $<TARGET_FILE:${target_name}>)

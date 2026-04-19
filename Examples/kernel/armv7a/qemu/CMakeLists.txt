cmake_minimum_required(VERSION 4.0)
project(charm-armv7a-qemu LANGUAGES C CXX ASM)

option(CHARM_QEMU_SEMIHOST_DEBUG "Enable semihosting debug breadcrumbs for QEMU runs" OFF)
set(CHARM_ARMV7A_ABORT_SMOKE "off" CACHE STRING "Optional abort smoke kind: off, data, data_align, prefetch, prefetch_xn, prefetch_page, prefetch_page_xn, prefetch_page_xn_runtime, data_perm, data_page, data_page_perm, data_page_perm_runtime")
set_property(CACHE CHARM_ARMV7A_ABORT_SMOKE PROPERTY STRINGS off data data_align prefetch prefetch_xn prefetch_page prefetch_page_xn prefetch_page_xn_runtime data_perm data_page data_page_perm data_page_perm_runtime)
set(CHARM_ARMV7A_EXCEPTION_SMOKE "off" CACHE STRING "Optional exception smoke kind: off, undefined")
set_property(CACHE CHARM_ARMV7A_EXCEPTION_SMOKE PROPERTY STRINGS off undefined)
set(CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE "off" CACHE STRING "Optional interrupt edge smoke kind: off, special_irq, sgi_irq_timeout, unexpected_irq, sgi_fiq_timeout")
set_property(CACHE CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE PROPERTY STRINGS off special_irq sgi_irq_timeout unexpected_irq sgi_fiq_timeout)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if (DEFINED CHARM_TARGET_ARCH AND NOT CHARM_TARGET_ARCH STREQUAL "armv7a")
    message(FATAL_ERROR
        "This example only supports CHARM_TARGET_ARCH=armv7a, got: ${CHARM_TARGET_ARCH}")
endif()
if (DEFINED CHARM_TARGET_PLATFORM AND NOT CHARM_TARGET_PLATFORM STREQUAL "qemu_virt")
    message(FATAL_ERROR
        "This example only supports CHARM_TARGET_PLATFORM=qemu_virt, got: ${CHARM_TARGET_PLATFORM}")
endif()
if (DEFINED CHARM_BAREMETAL AND NOT CHARM_BAREMETAL)
    message(FATAL_ERROR
        "This example requires CHARM_BAREMETAL=ON")
endif()
if (NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "off"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_align"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch_xn"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch_page"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch_page_xn"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch_page_xn_runtime"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_perm"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_page"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_page_perm"
    AND NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_page_perm_runtime")
    message(FATAL_ERROR
        "CHARM_ARMV7A_ABORT_SMOKE must be one of: off, data, data_align, prefetch, prefetch_xn, prefetch_page, prefetch_page_xn, prefetch_page_xn_runtime, data_perm, data_page, data_page_perm, data_page_perm_runtime")
endif()
if (NOT CHARM_ARMV7A_EXCEPTION_SMOKE STREQUAL "off"
    AND NOT CHARM_ARMV7A_EXCEPTION_SMOKE STREQUAL "undefined")
    message(FATAL_ERROR
        "CHARM_ARMV7A_EXCEPTION_SMOKE must be one of: off, undefined")
endif()
if (NOT CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE STREQUAL "off"
    AND NOT CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE STREQUAL "special_irq"
    AND NOT CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE STREQUAL "sgi_irq_timeout"
    AND NOT CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE STREQUAL "unexpected_irq"
    AND NOT CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE STREQUAL "sgi_fiq_timeout")
    message(FATAL_ERROR
        "CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE must be one of: off, special_irq, sgi_irq_timeout, unexpected_irq, sgi_fiq_timeout")
endif()
if (NOT CHARM_ARMV7A_ABORT_SMOKE STREQUAL "off"
    AND NOT CHARM_ARMV7A_EXCEPTION_SMOKE STREQUAL "off")
    message(FATAL_ERROR
        "Only one of CHARM_ARMV7A_ABORT_SMOKE or CHARM_ARMV7A_EXCEPTION_SMOKE may be enabled at a time")
endif()

get_filename_component(CHARM_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../.."
    ABSOLUTE)
include("${CHARM_ROOT}/cmake/CharmTargetConfig.cmake")

charm_add_config_interface(charm_cfg_armv7a_qemu
    ARCH armv7a
    PLATFORM qemu_virt
    BAREMETAL)

set(charm_armv7a_qemu_module_files
    "${CHARM_ROOT}/Modules/core/util/core.cppm"
    "${CHARM_ROOT}/Modules/core/trace_core.cppm"
    "${CHARM_ROOT}/Modules/core/util/expected.cppm"
    "${CHARM_ROOT}/Modules/core/util/error.cppm"
    "${CHARM_ROOT}/Modules/core/util/type_list.cppm"
    "${CHARM_ROOT}/Modules/core/util/variant.cppm"
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
    "${CHARM_ROOT}/Modules/system/kernel/runtime_service.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/runtime_trap.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/runtime_trap_ingress.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/scheduler.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/ssu.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_state.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_runtime_api.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_syscall_api.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_syscall_catalog.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_syscall_dispatch.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_syscall_table.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/task_syscall_frame.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/thread.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/timer.cppm"
    "${CHARM_ROOT}/Modules/system/kernel/trace.cppm"
    "${CHARM_ROOT}/targets/armv7a/common/armv7a_kernel_runtime_trap_frame_adapter.cppm"
    "${CHARM_ROOT}/targets/armv7a/common/armv7a_kernel_task_syscall_call_frame_adapter.cppm")

add_executable(${PROJECT_NAME})

target_sources(${PROJECT_NAME} PRIVATE
    "${CHARM_ROOT}/targets/armv7a/common/armv7a_handoff_contract.cpp"
    vectors.S
    startup.S
    main.cpp
    armv7a_bringup_phase.cpp
    armv7a_context_smoke.cpp
    armv7a_thread_runtime.cpp
    armv7a_context_switch.cpp
    armv7a_context_switch.S
    armv7a_cpu.cpp
    armv7a_gic.cpp
    armv7a_kernel_port.cpp
    armv7a_scheduler_dispatch.cpp
    armv7a_scheduler_tick.cpp
    armv7a_runtime_trap_frame.cpp
    armv7a_runtime_trap.cpp
    armv7a_runtime_trap_mapping.cpp
    armv7a_runtime_trap_adapter.cpp
    armv7a_runtime_trap_seam.cpp
    armv7a_runtime_trap_live_adapter.cpp
    armv7a_runtime_trap_ingress_adapter.cpp
    armv7a_runtime_trap_caller.cpp
    armv7a_runtime_trap_dispatch.cpp
    armv7a_runtime_current.cpp
    armv7a_runtime_trap_context.cpp
    armv7a_runtime_trap_roundtrip.cpp
    armv7a_runtime_trap_failure.cpp
    armv7a_runtime_bridge.cpp
    armv7a_runtime_loop.cpp
    armv7a_runtime_live.cpp
    armv7a_task_syscall_frame.cpp
    armv7a_task_syscall_dispatch.cpp
    armv7a_task_syscall_surface.cpp
    armv7a_task_syscall_ingress_adapter.cpp
    armv7a_task_syscall_caller.cpp
    armv7a_task_syscall_roundtrip.cpp
    armv7a_task_syscall_glue.cpp
    armv7a_task_syscall_failure.cpp
    armv7a_diag_console.cpp
    armv7a_diag_context.cpp
    armv7a_handler_stack.cpp
    armv7a_handoff_prepare.cpp
    armv7a_interrupt_smoke.cpp
    armv7a_interrupt_runtime_hook.cpp
    armv7a_interrupt_diagnostics.cpp
    armv7a_interrupt_observation_sequence.cpp
    armv7a_arch_timer.cpp
    armv7a_attribute_probe.cpp
    armv7a_cache.cpp
    armv7a_dcache_probe.cpp
    armv7a_icache_probe.S
    armv7a_icache_probe.cpp
    armv7a_mmu.cpp
    armv7a_boot_diagnostics.cpp
    armv7a_memory_probe_sequence.cpp
    armv7a_page_table_probe.cpp
    armv7a_section_split_probe.cpp
    armv7a_boot_page_table.cpp
    armv7a_translation_walk.cpp
    armv7a_abort_smoke.cpp
    armv7a_exception_smoke.cpp
    armv7a_small_page_probe.cpp
    early_uart.cpp
    qemu_virt_platform.cpp
    qemu_virt_platform_interrupts.cpp
    armv7a_exception_diagnostics.cpp
    exceptions.cpp
    irq_timer.cpp
    runtime_stubs.cpp)

target_sources(${PROJECT_NAME}
    PRIVATE
    FILE_SET modules TYPE CXX_MODULES
    BASE_DIRS
        "${CHARM_ROOT}/Modules"
        "${CHARM_ROOT}/targets"
    FILES
        ${charm_armv7a_qemu_module_files})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CHARM_ROOT}"
    "${CHARM_ROOT}/targets/armv7a/common")

target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=${CHARM_ARMV7A_CPU}
    -marm)

target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=${CHARM_ARMV7A_CPU}
    -marm
    -Wl,--build-id=none
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    -T${CMAKE_CURRENT_SOURCE_DIR}/linker.ld)

charm_apply_target_profile(${PROJECT_NAME}
    BAREMETAL)

charm_apply_config_targets(${PROJECT_NAME} charm_cfg_armv7a_qemu)

if (CHARM_QEMU_SEMIHOST_DEBUG)
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_QEMU_SEMIHOST_DEBUG=1)
endif()

if (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_DATA=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_align")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_DATA_ALIGN=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_PREFETCH=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch_xn")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_XN=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch_page")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch_page_xn")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "prefetch_page_xn_runtime")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN_RUNTIME=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_perm")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_DATA_PERM=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_page")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_page_perm")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM=1)
elseif (CHARM_ARMV7A_ABORT_SMOKE STREQUAL "data_page_perm_runtime")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM_RUNTIME=1)
endif()

if (CHARM_ARMV7A_EXCEPTION_SMOKE STREQUAL "undefined")
    target_compile_definitions(${PROJECT_NAME} PRIVATE CHARM_ARMV7A_EXCEPTION_SMOKE_UNDEFINED=1)
endif()

if (CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE STREQUAL "special_irq")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE_SPECIAL_IRQ=1)
elseif (CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE STREQUAL "sgi_irq_timeout")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE_SGI_IRQ_TIMEOUT=1)
elseif (CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE STREQUAL "unexpected_irq")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE_UNEXPECTED_IRQ=1)
elseif (CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE STREQUAL "sgi_fiq_timeout")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE_SGI_FIQ_TIMEOUT=1)
endif()

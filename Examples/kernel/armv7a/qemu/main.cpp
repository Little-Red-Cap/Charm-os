#include "armv7a_bringup_phase.hpp"
#include "armv7a_boot_diagnostics.hpp"
#include "armv7a_context_smoke.hpp"
#include "armv7a_handoff_prepare.hpp"
#include "armv7a_interrupt_observation_sequence.hpp"
#include "armv7a_kernel_port.hpp"
#include "armv7a_memory_probe_sequence.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge.hpp"
#include "armv7a_runtime_trap_frame.hpp"
#include "armv7a_runtime_trap.hpp"
#include "armv7a_runtime_trap_adapter.hpp"
#include "armv7a_runtime_trap_caller.hpp"
#include "armv7a_runtime_current.hpp"
#include "armv7a_runtime_trap_context.hpp"
#include "armv7a_runtime_trap_dispatch.hpp"
#include "armv7a_runtime_trap_ingress_adapter.hpp"
#include "armv7a_runtime_trap_live_adapter.hpp"
#include "armv7a_runtime_trap_roundtrip.hpp"
#include "armv7a_runtime_trap_seam.hpp"
#include "armv7a_runtime_trap_mapping.hpp"
#include "armv7a_task_syscall_frame.hpp"
#include "armv7a_task_syscall_dispatch.hpp"
#include "armv7a_task_syscall_surface.hpp"
#include "armv7a_task_syscall_ingress_adapter.hpp"
#include "armv7a_task_syscall_caller.hpp"
#include "armv7a_task_syscall_glue.hpp"
#include "armv7a_task_syscall_failure.hpp"
#include "armv7a_task_syscall_roundtrip.hpp"
#include "armv7a_scheduler_dispatch.hpp"
#include "armv7a_scheduler_tick.hpp"

int main()
{
#if defined(CHARM_QEMU_SEMIHOST_DEBUG)
    armv7a_platform_debug_trace("semihost: entering main\n");
#endif
    armv7a_platform_early_console_init();
    armv7a_platform_early_console_puts("Charm ARMv7-A QEMU skeleton\r\n");
    armv7a_platform_early_console_puts("Targeting Cortex-A7 first, RK3506 later.\r\n");
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kBootCpuState);
    armv7a_print_boot_cpu_state();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kBootCpuState);
    armv7a_prepare_memory_probe_environment();
    armv7a_print_memory_probe_environment();
    armv7a_activate_memory_probe_environment();
    armv7a_run_pre_dcache_probe_sequence();
    armv7a_run_post_dcache_probe_sequence();
    armv7a_run_interrupt_observation_sequence();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kKernelIngress);
    armv7a_print_kernel_port_status();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kKernelIngress);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSchedulerTickIngress);
    armv7a_print_scheduler_tick_ingress();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kSchedulerTickIngress);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapFrame);
    armv7a_print_runtime_trap_frame_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapFrame);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapIngress);
    armv7a_print_runtime_trap_ingress();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapIngress);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapMapping);
    armv7a_print_runtime_trap_mapping_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapMapping);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapAdapter);
    armv7a_print_runtime_trap_adapter_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapAdapter);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapSeam);
    armv7a_print_runtime_trap_seam_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapSeam);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapLiveAdapter);
    armv7a_print_runtime_trap_live_adapter_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapLiveAdapter);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapIngressAdapter);
    armv7a_print_runtime_trap_ingress_adapter_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapIngressAdapter);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapCaller);
    armv7a_print_runtime_trap_caller_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapCaller);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapDispatch);
    armv7a_print_runtime_trap_dispatch_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapDispatch);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeCurrent);
    armv7a_print_runtime_current_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeCurrent);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapContext);
    armv7a_print_runtime_trap_context_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapContext);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeTrapRoundtrip);
    armv7a_print_runtime_trap_roundtrip_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeTrapRoundtrip);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kContextSwitchSmoke);
    armv7a_run_context_switch_smoke();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kContextSwitchSmoke);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSchedulerDispatch);
    armv7a_print_scheduler_dispatch_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kSchedulerDispatch);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kRuntimeBridge);
    armv7a_print_runtime_bridge_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kRuntimeBridge);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kTaskSyscallFrame);
    armv7a_print_task_syscall_frame_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kTaskSyscallFrame);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kTaskSyscallDispatch);
    armv7a_print_task_syscall_dispatch_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kTaskSyscallDispatch);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kTaskSyscallSurface);
    armv7a_print_task_syscall_surface_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kTaskSyscallSurface);
    armv7a_enter_bringup_phase(
        Armv7aBringupPhase::kTaskSyscallIngressAdapter);
    armv7a_print_task_syscall_ingress_adapter_observation();
    armv7a_complete_bringup_phase(
        Armv7aBringupPhase::kTaskSyscallIngressAdapter);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kTaskSyscallCaller);
    armv7a_print_task_syscall_caller_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kTaskSyscallCaller);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kTaskSyscallRoundtrip);
    armv7a_print_task_syscall_roundtrip_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kTaskSyscallRoundtrip);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kTaskSyscallGlue);
    armv7a_print_task_syscall_glue_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kTaskSyscallGlue);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kTaskSyscallFailure);
    armv7a_print_task_syscall_failure_observation();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kTaskSyscallFailure);
    armv7a_run_handoff_prepare_dry_run();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kIdle);
    armv7a_platform_idle_forever();
}

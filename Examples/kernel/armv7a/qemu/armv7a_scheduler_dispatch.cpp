#include "armv7a_scheduler_dispatch.hpp"

#include "armv7a_context_smoke.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_kernel_port.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"
#include "armv7a_runtime_current.hpp"
#include "armv7a_runtime_trap_dispatch.hpp"
#include "armv7a_scheduler_tick.hpp"

namespace {
struct Armv7aSchedulerDispatchTaskSample {
    bool recorded = false;
    Armv7aRuntimeCurrentContext current{};
    bool task_dispatch_ready = false;
    bool current_seen = false;
    bool current_task_matches = false;
    bool current_stack_matches = false;
};

Armv7aSchedulerDispatchTaskSample g_armv7aSchedulerDispatchTaskSample{};

const char* armv7a_scheduler_dispatch_path_name(
    Armv7aSchedulerDispatchPath path) noexcept
{
    switch (path) {
    case Armv7aSchedulerDispatchPath::svc_trap:
        return "svc-trap";
    case Armv7aSchedulerDispatchPath::timer_tick:
        return "timer-tick";
    case Armv7aSchedulerDispatchPath::none:
    default:
        return "none";
    }
}

const Armv7aRuntimeTrapDispatchObservation&
armv7a_select_scheduler_task_dispatch_observation(
    const Armv7aSvcObservation& svc,
    const Armv7aRuntimeTrapDispatchPairObservation& dispatch) noexcept
{
    if (armv7a_svc_service_matches(
            svc, kArmv7aRuntimeBridgeSleepServiceId)) {
        return dispatch.sleep;
    }

    return dispatch.yield;
}

Armv7aSchedulerDispatchTaskSample armv7a_capture_scheduler_dispatch_task_sample()
    noexcept
{
    const auto svc = armv7a_svc_last_observation();
    const auto dispatch = armv7a_capture_runtime_trap_dispatch_observation();
    const auto& task_dispatch =
        armv7a_select_scheduler_task_dispatch_observation(svc, dispatch);
    Armv7aRuntimeCurrentContext current{};
    const auto current_seen =
        armv7a_capture_runtime_current_sample_context(current);

    return Armv7aSchedulerDispatchTaskSample{
        .recorded = true,
        .current = current,
        .task_dispatch_ready =
            armv7a_runtime_trap_dispatch_core_ready(task_dispatch),
        .current_seen = current_seen,
        .current_task_matches =
            current_seen && task_dispatch.frame_view.task_valid &&
            current.task_valid &&
            task_dispatch.frame_view.task == current.task &&
            task_dispatch.frame_view.task_valid == current.task_valid,
        .current_stack_matches =
            current_seen &&
            task_dispatch.frame_view.stack_pointer == current.stack_pointer,
    };
}
} // namespace

void armv7a_record_scheduler_dispatch_task_sample() noexcept
{
    g_armv7aSchedulerDispatchTaskSample =
        armv7a_capture_scheduler_dispatch_task_sample();
}

Armv7aSchedulerDispatchObservation
armv7a_capture_scheduler_dispatch_observation() noexcept
{
    const auto contract = armv7a_make_qemu_kernel_port_contract();
    const auto svc = armv7a_svc_last_observation();
    const auto tick = armv7a_capture_scheduler_tick_ingress();
    const auto context = armv7a_context_switch_smoke_last_observation();
    const auto sample = g_armv7aSchedulerDispatchTaskSample;

    return Armv7aSchedulerDispatchObservation{
        .task_path = armv7a_svc_observation_observed(svc)
            ? Armv7aSchedulerDispatchPath::svc_trap
            : Armv7aSchedulerDispatchPath::none,
        .isr_path = armv7a_scheduler_tick_source_matches_timer(tick)
            ? Armv7aSchedulerDispatchPath::timer_tick
            : Armv7aSchedulerDispatchPath::none,
        .current = sample.current,
        .context_switch_ready = armv7a_kernel_context_port_ready(contract.context),
        .context_round_trip = context.round_trip,
        .task_dispatch_ready =
            sample.recorded && sample.task_dispatch_ready,
        .current_seen = sample.recorded && sample.current_seen,
        .current_task_matches =
            sample.recorded && sample.current_task_matches,
        .current_stack_matches =
            sample.recorded && sample.current_stack_matches,
        .task = svc,
        .isr = tick,
    };
}

void armv7a_print_scheduler_dispatch_observation()
{
    const auto observation = armv7a_capture_scheduler_dispatch_observation();

    armv7a_platform_early_console_puts("ARMv7-A scheduler dispatch, task=");
    armv7a_platform_early_console_puts(
        armv7a_scheduler_dispatch_path_name(observation.task_path));
    armv7a_platform_early_console_puts(", isr=");
    armv7a_platform_early_console_puts(
        armv7a_scheduler_dispatch_path_name(observation.isr_path));
    armv7a_platform_early_console_puts(", task-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_task_path_ready(observation)));
    armv7a_platform_early_console_puts(", isr-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_isr_path_ready(observation)));
    armv7a_platform_early_console_puts(", context-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.context_switch_ready));
    armv7a_platform_early_console_puts(", round-trip=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.context_round_trip));
    armv7a_platform_early_console_puts(", current=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_dispatch_current_ready(observation)));
    armv7a_platform_early_console_puts(", dispatch=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_scheduler_dispatch_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

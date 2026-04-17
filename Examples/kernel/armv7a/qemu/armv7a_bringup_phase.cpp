#include "armv7a_bringup_phase.hpp"

#include "armv7a_platform.hpp"

namespace {
Armv7aBringupPhase g_armv7a_bringup_phase = Armv7aBringupPhase::kReset;
Armv7aBringupPhase g_armv7a_last_completed_phase = Armv7aBringupPhase::kReset;
}

const char* armv7a_bringup_phase_name(Armv7aBringupPhase phase)
{
    switch (phase) {
    case Armv7aBringupPhase::kReset:
        return "reset";
    case Armv7aBringupPhase::kBootCpuState:
        return "boot-cpu-state";
    case Armv7aBringupPhase::kMemoryProbePrepare:
        return "memory-probe-prepare";
    case Armv7aBringupPhase::kMemoryProbeDescribe:
        return "memory-probe-describe";
    case Armv7aBringupPhase::kMmuActivate:
        return "mmu-activate";
    case Armv7aBringupPhase::kSmallPageProbe:
        return "small-page-probe";
    case Armv7aBringupPhase::kAttributeProbe:
        return "attribute-probe";
    case Armv7aBringupPhase::kIcacheProbe:
        return "icache-probe";
    case Armv7aBringupPhase::kAbortSmoke:
        return "abort-smoke";
    case Armv7aBringupPhase::kExceptionSmoke:
        return "exception-smoke";
    case Armv7aBringupPhase::kDcacheProbe:
        return "dcache-probe";
    case Armv7aBringupPhase::kPageTableProbe:
        return "page-table-probe";
    case Armv7aBringupPhase::kSectionSplitProbe:
        return "section-split-probe";
    case Armv7aBringupPhase::kSvcSmoke:
        return "svc-smoke";
    case Armv7aBringupPhase::kTimerIrqSmoke:
        return "timer-irq-smoke";
    case Armv7aBringupPhase::kSgiIrqSmoke:
        return "sgi-irq-smoke";
    case Armv7aBringupPhase::kSgiFiqSmoke:
        return "sgi-fiq-smoke";
    case Armv7aBringupPhase::kSpecialIrqSmoke:
        return "special-irq-smoke";
    case Armv7aBringupPhase::kSgiIrqTimeoutSmoke:
        return "sgi-irq-timeout-smoke";
    case Armv7aBringupPhase::kUnexpectedIrqSmoke:
        return "unexpected-irq-smoke";
    case Armv7aBringupPhase::kSgiFiqTimeoutSmoke:
        return "sgi-fiq-timeout-smoke";
    case Armv7aBringupPhase::kKernelIngress:
        return "kernel-ingress";
    case Armv7aBringupPhase::kSchedulerTickIngress:
        return "scheduler-tick-ingress";
    case Armv7aBringupPhase::kRuntimeTrapFrame:
        return "runtime-trap-frame";
    case Armv7aBringupPhase::kRuntimeTrapIngress:
        return "runtime-trap-ingress";
    case Armv7aBringupPhase::kRuntimeTrapMapping:
        return "runtime-trap-mapping";
    case Armv7aBringupPhase::kRuntimeTrapAdapter:
        return "runtime-trap-adapter";
    case Armv7aBringupPhase::kRuntimeTrapSeam:
        return "runtime-trap-seam";
    case Armv7aBringupPhase::kRuntimeTrapCaller:
        return "runtime-trap-caller";
    case Armv7aBringupPhase::kContextSwitchSmoke:
        return "context-switch-smoke";
    case Armv7aBringupPhase::kSchedulerDispatch:
        return "scheduler-dispatch";
    case Armv7aBringupPhase::kRuntimeBridge:
        return "runtime-bridge";
    case Armv7aBringupPhase::kHandoffPrepare:
        return "handoff-prepare";
    case Armv7aBringupPhase::kIdle:
        return "idle";
    default:
        return "unknown";
    }
}

Armv7aBringupPhase armv7a_current_bringup_phase()
{
    return g_armv7a_bringup_phase;
}

Armv7aBringupPhase armv7a_last_completed_bringup_phase()
{
    return g_armv7a_last_completed_phase;
}

void armv7a_enter_bringup_phase(Armv7aBringupPhase phase)
{
    if (g_armv7a_bringup_phase == phase) {
        return;
    }

    g_armv7a_bringup_phase = phase;
    armv7a_platform_early_console_puts("ARMv7-A phase, stage=");
    armv7a_platform_early_console_puts(armv7a_bringup_phase_name(phase));
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_complete_bringup_phase(Armv7aBringupPhase phase)
{
    if (g_armv7a_last_completed_phase == phase) {
        return;
    }

    g_armv7a_last_completed_phase = phase;
    armv7a_platform_early_console_puts("ARMv7-A phase complete, stage=");
    armv7a_platform_early_console_puts(armv7a_bringup_phase_name(phase));
    armv7a_platform_early_console_puts("\r\n");
}

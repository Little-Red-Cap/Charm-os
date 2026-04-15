#include "armv7a_bringup_phase.hpp"
#include "armv7a_interrupt_observation_sequence.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_interrupt_diagnostics.hpp"
#include "armv7a_interrupt_smoke.hpp"

void armv7a_run_interrupt_observation_sequence()
{
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSvcSmoke);
    armv7a_svc_smoke_test();
    const auto svc_observation = armv7a_svc_last_observation();
    if (svc_observation.seen) {
        armv7a_print_return_state_evidence(
            "svc", svc_observation.origin_spsr, armv7a_read_cpsr());
    }

    armv7a_enter_bringup_phase(Armv7aBringupPhase::kTimerIrqSmoke);
    armv7a_irq_smoke_test();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSgiIrqSmoke);
    armv7a_sgi_smoke_test();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSgiFiqSmoke);
    armv7a_fiq_smoke_test();
    armv7a_interrupt_print_security_side_evidence();
}

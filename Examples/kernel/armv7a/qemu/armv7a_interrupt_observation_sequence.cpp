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
    if (armv7a_svc_observation_observed(svc_observation)) {
        armv7a_print_return_state_evidence(
            "svc", svc_observation.origin_spsr, armv7a_read_cpsr());
    }
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kSvcSmoke);

    armv7a_enter_bringup_phase(Armv7aBringupPhase::kTimerIrqSmoke);
    armv7a_irq_smoke_test();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kTimerIrqSmoke);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSgiIrqSmoke);
    armv7a_sgi_smoke_test();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kSgiIrqSmoke);
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSgiFiqSmoke);
    armv7a_fiq_smoke_test();
    armv7a_interrupt_print_security_side_evidence();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kSgiFiqSmoke);

#if defined(CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE_SPECIAL_IRQ)
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSpecialIrqSmoke);
    armv7a_special_irq_ack_smoke_test();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kSpecialIrqSmoke);
#elif defined(CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE_SGI_IRQ_TIMEOUT)
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSgiIrqTimeoutSmoke);
    armv7a_sgi_irq_timeout_smoke_test();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kSgiIrqTimeoutSmoke);
#elif defined(CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE_UNEXPECTED_IRQ)
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kUnexpectedIrqSmoke);
    armv7a_unexpected_irq_smoke_test();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kUnexpectedIrqSmoke);
#elif defined(CHARM_ARMV7A_INTERRUPT_EDGE_SMOKE_SGI_FIQ_TIMEOUT)
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSgiFiqTimeoutSmoke);
    armv7a_sgi_fiq_timeout_smoke_test();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kSgiFiqTimeoutSmoke);
#endif
}

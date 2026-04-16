#pragma once

#include "armv7a_interrupt_completion_contract.hpp"
#include "armv7a_interrupt_contract.hpp"
#include "armv7a_interrupt_lifecycle_contract.hpp"

#include <cstdint>

struct Armv7aExceptionFrame;
struct Armv7aInterruptObservation;

Armv7aTimerPendingSnapshot armv7a_capture_timer_pending_snapshot();
Armv7aSgiPendingSnapshot armv7a_capture_sgi_pending_snapshot();
Armv7aSgiPendingSnapshot armv7a_capture_sgi_pending_snapshot(unsigned int intid);
Armv7aTimerTimeoutSnapshot armv7a_capture_timer_timeout_snapshot(bool pending_observed);
Armv7aSgiTimeoutSnapshot armv7a_capture_sgi_timeout_snapshot(bool pending_observed);
Armv7aSgiTimeoutSnapshot armv7a_capture_sgi_timeout_snapshot(unsigned int intid,
                                                              bool pending_observed);

void armv7a_interrupt_print_reset_state();
void armv7a_interrupt_print_timer_pending_evidence(const Armv7aTimerPendingSnapshot& snapshot);
void armv7a_interrupt_print_sgi_pending_evidence(const Armv7aSgiPendingSnapshot& snapshot,
                                                 Armv7aPlatformInterruptRoute route);
void armv7a_interrupt_print_unexpected_pending_evidence(const Armv7aSgiPendingSnapshot& snapshot,
                                                        Armv7aPlatformInterruptRoute route);
void armv7a_interrupt_print_active(const char* label,
                                   const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_completion(
    const char* label,
    const Armv7aInterruptCompletionObservation& observation);
void armv7a_interrupt_print_lifecycle(
    const char* label,
    const Armv7aInterruptLifecycleObservation& observation);
void armv7a_interrupt_print_special_ack(const char* label,
                                        Armv7aPlatformInterruptRoute route,
                                        const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_timeout_summary(const char* expected,
                                            Armv7aPlatformInterruptRoute route,
                                            const Armv7aInterruptTimeoutContext& context,
                                            const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_observed_intid(const char* label, unsigned int intid);
void armv7a_interrupt_print_unexpected(const char* label,
                                       const Armv7aInterruptObservation& observation,
                                       const Armv7aExceptionFrame& frame);
void armv7a_interrupt_print_irq_timeout(const Armv7aTimerTimeoutSnapshot& snapshot,
                                        const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_sgi_timeout(const Armv7aSgiTimeoutSnapshot& snapshot,
                                        const Armv7aInterruptObservation& observation,
                                        Armv7aPlatformInterruptRoute route);
void armv7a_interrupt_print_unexpected_irq_timeout(
    const Armv7aSgiTimeoutSnapshot& snapshot,
    const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_fiq_timeout(const Armv7aSgiTimeoutSnapshot& snapshot,
                                        const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_security_side_evidence();

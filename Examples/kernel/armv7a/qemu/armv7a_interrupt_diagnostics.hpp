#pragma once

#include <cstdint>

#include "armv7a_platform.hpp"

struct Armv7aExceptionFrame;
struct Armv7aInterruptObservation;

struct Armv7aTimerPendingSnapshot {
    std::uint32_t timer_ctrl = 0u;
    Armv7aPlatformInterruptLineState secure_line{};
    Armv7aPlatformInterruptLineState nonsecure_line{};
    Armv7aPlatformInterruptControllerState controller{};
};

struct Armv7aSgiPendingSnapshot {
    Armv7aPlatformInterruptLineState line{};
    Armv7aPlatformInterruptControllerState controller{};
};

struct Armv7aInterruptTimeoutContext {
    bool pending_observed = false;
    std::uint32_t current_cpsr = 0u;
    Armv7aPlatformInterruptControllerState controller{};
};

struct Armv7aTimerTimeoutSnapshot {
    Armv7aInterruptTimeoutContext context{};
    std::uint32_t timer_ctrl = 0u;
    Armv7aPlatformInterruptLineState secure_line{};
    Armv7aPlatformInterruptLineState nonsecure_line{};
};

struct Armv7aSgiTimeoutSnapshot {
    Armv7aInterruptTimeoutContext context{};
    Armv7aPlatformInterruptLineState line{};
};

Armv7aTimerPendingSnapshot armv7a_capture_timer_pending_snapshot();
Armv7aSgiPendingSnapshot armv7a_capture_sgi_pending_snapshot();
Armv7aTimerTimeoutSnapshot armv7a_capture_timer_timeout_snapshot(bool pending_observed);
Armv7aSgiTimeoutSnapshot armv7a_capture_sgi_timeout_snapshot(bool pending_observed);
bool armv7a_timer_pending_observed(const Armv7aTimerPendingSnapshot& snapshot);
bool armv7a_sgi_pending_observed(const Armv7aSgiPendingSnapshot& snapshot);

void armv7a_interrupt_print_reset_state();
void armv7a_interrupt_print_timer_pending_evidence(const Armv7aTimerPendingSnapshot& snapshot);
void armv7a_interrupt_print_sgi_pending_evidence(const Armv7aSgiPendingSnapshot& snapshot,
                                                 Armv7aPlatformInterruptRoute route);
void armv7a_interrupt_print_active(const char* label,
                                   const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_special_ack(const char* label,
                                        Armv7aPlatformInterruptRoute route,
                                        const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_timeout_summary(const char* expected,
                                            Armv7aPlatformInterruptRoute route,
                                            const Armv7aInterruptTimeoutContext& context,
                                            const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_observed_intid(const char* label, unsigned int intid);
void armv7a_interrupt_print_unexpected(const char* label,
                                       unsigned int intid,
                                       const Armv7aExceptionFrame& frame);
void armv7a_interrupt_print_irq_timeout(const Armv7aTimerTimeoutSnapshot& snapshot,
                                        const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_sgi_timeout(const Armv7aSgiTimeoutSnapshot& snapshot,
                                        const Armv7aInterruptObservation& observation,
                                        Armv7aPlatformInterruptRoute route);
void armv7a_interrupt_print_fiq_timeout(const Armv7aSgiTimeoutSnapshot& snapshot,
                                        const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_security_side_evidence();

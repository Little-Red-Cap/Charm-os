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

Armv7aTimerPendingSnapshot armv7a_capture_timer_pending_snapshot();
Armv7aSgiPendingSnapshot armv7a_capture_sgi_pending_snapshot();
bool armv7a_timer_pending_observed(const Armv7aTimerPendingSnapshot& snapshot);
bool armv7a_sgi_pending_observed(const Armv7aSgiPendingSnapshot& snapshot);

void armv7a_interrupt_print_timer_pending_evidence(const Armv7aTimerPendingSnapshot& snapshot);
void armv7a_interrupt_print_sgi_pending_evidence(const Armv7aSgiPendingSnapshot& snapshot,
                                                 Armv7aPlatformInterruptRoute route);
void armv7a_interrupt_print_active(const char* label,
                                   const Armv7aInterruptObservation& observation);
void armv7a_interrupt_print_observed_intid(const char* label, unsigned int intid);
void armv7a_interrupt_print_unexpected(const char* label,
                                       unsigned int intid,
                                       const Armv7aExceptionFrame& frame);
void armv7a_interrupt_print_irq_timeout(std::uint32_t timer_ctrl);
void armv7a_interrupt_print_sgi_timeout();
void armv7a_interrupt_print_fiq_timeout();
void armv7a_interrupt_print_security_side_evidence();

#pragma once

#include <cstdint>

#include "armv7a_interrupt_completion_contract.hpp"
#include "armv7a_platform.hpp"

struct Armv7aExceptionFrame;

enum class Armv7aInterruptSmokeKind : std::uint8_t {
    kNone = 0,
    kTimerIrq = 1,
    kSgiIrq = 2,
    kSgiFiq = 3,
    kSpecialIrq = 4,
    kSgiIrqTimeout = 5,
    kUnexpectedIrq = 6,
    kSgiFiqTimeout = 7,
};

void armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind kind);
void armv7a_interrupt_smoke_finish();

bool armv7a_interrupt_smoke_seen();
Armv7aInterruptObservation armv7a_interrupt_smoke_last_observation();
Armv7aInterruptObservation armv7a_interrupt_smoke_observation(Armv7aInterruptSmokeKind kind);
Armv7aInterruptCompletionObservation armv7a_interrupt_smoke_last_completion();
Armv7aInterruptCompletionObservation armv7a_interrupt_smoke_completion(
    Armv7aInterruptSmokeKind kind);

void armv7a_handle_irq_synthetic(Armv7aExceptionFrame* frame);

extern "C" void armv7a_handle_irq(Armv7aExceptionFrame* frame);
extern "C" void armv7a_handle_fiq(Armv7aExceptionFrame* frame);
extern "C" void armv7a_irq_smoke_test();
extern "C" void armv7a_sgi_smoke_test();
extern "C" void armv7a_fiq_smoke_test();
extern "C" void armv7a_special_irq_ack_smoke_test();
extern "C" void armv7a_sgi_irq_timeout_smoke_test();
extern "C" void armv7a_unexpected_irq_smoke_test();
extern "C" void armv7a_sgi_fiq_timeout_smoke_test();

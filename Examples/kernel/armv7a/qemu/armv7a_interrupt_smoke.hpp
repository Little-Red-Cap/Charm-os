#pragma once

#include <cstdint>

struct Armv7aExceptionFrame;

enum class Armv7aInterruptSmokeKind : std::uint8_t {
    kNone = 0,
    kTimerIrq = 1,
    kSgiIrq = 2,
    kSgiFiq = 3,
};

struct Armv7aInterruptObservation {
    bool seen = false;
    unsigned int intid = 0u;
    std::uint32_t handler_cpsr = 0u;
    std::uint32_t handler_spsr = 0u;
    std::uint32_t return_pc = 0u;
};

void armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind kind);
void armv7a_interrupt_smoke_finish();

bool armv7a_interrupt_smoke_seen();
Armv7aInterruptObservation armv7a_interrupt_smoke_last_observation();
Armv7aInterruptObservation armv7a_interrupt_smoke_observation(Armv7aInterruptSmokeKind kind);

extern "C" void armv7a_handle_irq(Armv7aExceptionFrame* frame);
extern "C" void armv7a_handle_fiq(Armv7aExceptionFrame* frame);

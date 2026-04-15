#pragma once

#include <cstdint>

#include "armv7a_platform.hpp"

struct Armv7aExceptionFrame;

enum class Armv7aInterruptSmokeKind : std::uint8_t {
    kNone = 0,
    kTimerIrq = 1,
    kSgiIrq = 2,
    kSgiFiq = 3,
    kSpecialIrq = 4,
};

struct Armv7aInterruptObservation {
    bool seen = false;
    bool special = false;
    bool synthetic = false;
    unsigned int intid = 0u;
    std::uint32_t raw_acknowledge = 0u;
    Armv7aPlatformInterruptControllerState controller{};
    Armv7aPlatformInterruptLineState line{};
    std::uint32_t handler_cpsr = 0u;
    std::uint32_t handler_spsr = 0u;
    std::uint32_t return_pc = 0u;
};

void armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind kind);
void armv7a_interrupt_smoke_finish();

bool armv7a_interrupt_smoke_seen();
Armv7aInterruptObservation armv7a_interrupt_smoke_last_observation();
Armv7aInterruptObservation armv7a_interrupt_smoke_observation(Armv7aInterruptSmokeKind kind);

void armv7a_handle_irq_synthetic(Armv7aExceptionFrame* frame);

extern "C" void armv7a_handle_irq(Armv7aExceptionFrame* frame);
extern "C" void armv7a_handle_fiq(Armv7aExceptionFrame* frame);
extern "C" void armv7a_irq_smoke_test();
extern "C" void armv7a_sgi_smoke_test();
extern "C" void armv7a_fiq_smoke_test();
extern "C" void armv7a_special_irq_ack_smoke_test();

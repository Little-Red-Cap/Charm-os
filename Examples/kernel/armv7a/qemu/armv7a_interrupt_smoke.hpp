#pragma once

#include <cstdint>

struct Armv7aExceptionFrame;

enum class Armv7aInterruptSmokeKind : std::uint8_t {
    kNone = 0,
    kTimerIrq = 1,
    kSgiIrq = 2,
    kSgiFiq = 3,
};

void armv7a_interrupt_smoke_begin(Armv7aInterruptSmokeKind kind);
void armv7a_interrupt_smoke_finish();

bool armv7a_interrupt_smoke_seen();
unsigned int armv7a_interrupt_smoke_last_intid();
std::uint32_t armv7a_interrupt_smoke_last_handler_cpsr();
std::uint32_t armv7a_interrupt_smoke_last_handler_spsr();

void armv7a_interrupt_print_irq_timeout(std::uint32_t timer_ctrl);
void armv7a_interrupt_print_sgi_timeout();
void armv7a_interrupt_print_fiq_timeout();

extern "C" void armv7a_handle_irq(Armv7aExceptionFrame* frame);
extern "C" void armv7a_handle_fiq(Armv7aExceptionFrame* frame);

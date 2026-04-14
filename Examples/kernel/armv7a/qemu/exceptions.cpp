#include <cstdint>

#include "armv7a_cpu.hpp"
#include "armv7a_exception_frame.hpp"
#include "armv7a_mmu.hpp"

extern "C" void early_uart_init();
extern "C" void early_uart_putc(char ch);
extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
void early_uart_write_hex(std::uint32_t value, int digits)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        early_uart_putc(kHex[(value >> shift) & 0x0Fu]);
    }
}

const char* exception_name(Armv7aExceptionKind kind)
{
    switch (kind) {
    case kArmv7aExceptionUndefined:
        return "undefined";
    case kArmv7aExceptionPrefetchAbort:
        return "prefetch abort";
    case kArmv7aExceptionDataAbort:
        return "data abort";
    case kArmv7aExceptionReserved:
        return "reserved vector";
    case kArmv7aExceptionIrq:
        return "irq";
    case kArmv7aExceptionFiq:
        return "fiq";
    case kArmv7aExceptionSvc:
        return "svc";
    default:
        return "unknown";
    }
}

void print_fault_registers(Armv7aExceptionKind kind)
{
    switch (kind) {
    case kArmv7aExceptionPrefetchAbort:
        early_uart_puts("ARMv7-A prefetch fault, ifsr=0x");
        early_uart_write_hex(armv7a_read_ifsr(), 8);
        early_uart_puts(", ifar=0x");
        early_uart_write_hex(armv7a_read_ifar(), 8);
        early_uart_puts(", aifsr=0x");
        early_uart_write_hex(armv7a_read_aifsr(), 8);
        early_uart_puts("\r\n");
        break;
    case kArmv7aExceptionDataAbort:
        early_uart_puts("ARMv7-A data fault, dfsr=0x");
        early_uart_write_hex(armv7a_read_dfsr(), 8);
        early_uart_puts(", dfar=0x");
        early_uart_write_hex(armv7a_read_dfar(), 8);
        early_uart_puts(", adfsr=0x");
        early_uart_write_hex(armv7a_read_adfsr(), 8);
        early_uart_puts("\r\n");
        break;
    default:
        break;
    }
}
} // namespace

extern "C" void armv7a_handle_svc(Armv7aExceptionFrame* frame)
{
    const auto* instruction =
        reinterpret_cast<const std::uint32_t*>(armv7a_exception_pc(*frame));
    const auto immediate = *instruction & 0x00FFFFFFu;
    early_uart_puts("ARMv7-A SVC vector active, imm=0x");
    early_uart_write_hex(immediate, 6);
    early_uart_puts("\r\n");
}

extern "C" [[noreturn]] void armv7a_exception_fatal(const Armv7aExceptionFrame* frame)
{
    const auto kind = armv7a_exception_kind(*frame);
    early_uart_init();
    early_uart_puts("ARMv7-A exception: ");
    early_uart_puts(exception_name(kind));
    early_uart_puts(", pc=0x");
    early_uart_write_hex(armv7a_exception_pc(*frame), 8);
    early_uart_puts(", lr=0x");
    early_uart_write_hex(frame->lr, 8);
    early_uart_puts(", spsr=0x");
    early_uart_write_hex(frame->spsr, 8);
    early_uart_puts(", mode=");
    early_uart_puts(armv7a_mode_name(frame->spsr));
    early_uart_puts("\r\n");
    print_fault_registers(kind);
    charm_spin();
}

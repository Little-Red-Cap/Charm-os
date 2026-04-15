#include "armv7a_handler_stack.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_platform.hpp"

namespace {
void print_hex32(std::uintptr_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    for (int shift = 28; shift >= 0; shift -= 4) {
        armv7a_platform_early_console_putc(kHex[(value >> shift) & 0x0Fu]);
    }
}

const char* yes_no_name(bool value)
{
    return value ? "yes" : "no";
}
} // namespace

void armv7a_print_handler_stack_evidence(const char* vector_tag, std::uint32_t current_cpsr)
{
    const auto sp = armv7a_read_sp();
    const auto range = armv7a_platform_stack_range_for_mode(current_cpsr);
    const auto in_range = range.base != 0u && range.top != 0u && sp >= range.base && sp <= range.top;
    std::uintptr_t used = 0u;
    if (range.top >= sp) {
        used = range.top - sp;
    }

    armv7a_platform_early_console_puts("ARMv7-A handler stack, vector=");
    armv7a_platform_early_console_puts(vector_tag);
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(current_cpsr));
    armv7a_platform_early_console_puts(", sp=0x");
    print_hex32(sp);
    armv7a_platform_early_console_puts(", base=0x");
    print_hex32(range.base);
    armv7a_platform_early_console_puts(", top=0x");
    print_hex32(range.top);
    armv7a_platform_early_console_puts(", used=0x");
    print_hex32(used);
    armv7a_platform_early_console_puts(", in-range=");
    armv7a_platform_early_console_puts(yes_no_name(in_range));
    armv7a_platform_early_console_puts("\r\n");
}

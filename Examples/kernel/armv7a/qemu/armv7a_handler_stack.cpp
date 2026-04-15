#include "armv7a_handler_stack.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"

namespace {
void print_stack_range_fields(std::uintptr_t sp, const Armv7aPlatformStackRange& range)
{
    const auto in_range = range.base != 0u && range.top != 0u && sp >= range.base && sp <= range.top;
    std::uintptr_t used = 0u;
    if (range.top >= sp) {
        used = range.top - sp;
    }

    armv7a_platform_early_console_puts(", sp=0x");
    armv7a_diag_put_hex(sp);
    armv7a_platform_early_console_puts(", base=0x");
    armv7a_diag_put_hex(range.base);
    armv7a_platform_early_console_puts(", top=0x");
    armv7a_diag_put_hex(range.top);
    armv7a_platform_early_console_puts(", used=0x");
    armv7a_diag_put_hex(used);
    armv7a_platform_early_console_puts(", in-range=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(in_range));
}
} // namespace

void armv7a_print_handler_stack_evidence(const char* vector_tag, std::uint32_t current_cpsr)
{
    const auto sp = armv7a_read_sp();
    const auto range = armv7a_platform_stack_range_for_mode(current_cpsr);

    armv7a_platform_early_console_puts("ARMv7-A handler stack, vector=");
    armv7a_platform_early_console_puts(vector_tag);
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(current_cpsr));
    print_stack_range_fields(sp, range);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_print_return_state_evidence(const char* vector_tag,
                                        std::uint32_t origin_psr,
                                        std::uint32_t current_cpsr)
{
    const auto sp = armv7a_read_sp();
    const auto range = armv7a_platform_stack_range_for_mode(current_cpsr);

    armv7a_platform_early_console_puts("ARMv7-A return evidence, vector=");
    armv7a_platform_early_console_puts(vector_tag);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(origin_psr));
    armv7a_platform_early_console_puts(", current-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(current_cpsr));
    armv7a_platform_early_console_puts(", origin-irq=");
    armv7a_platform_early_console_puts(armv7a_irq_masked(origin_psr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", current-irq=");
    armv7a_platform_early_console_puts(armv7a_irq_masked(current_cpsr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", origin-fiq=");
    armv7a_platform_early_console_puts(armv7a_fiq_masked(origin_psr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", current-fiq=");
    armv7a_platform_early_console_puts(armv7a_fiq_masked(current_cpsr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", mode-restored=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_mode_name(origin_psr) == armv7a_mode_name(current_cpsr)));
    armv7a_platform_early_console_puts(", irq-restored=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_irq_masked(origin_psr) == armv7a_irq_masked(current_cpsr)));
    armv7a_platform_early_console_puts(", fiq-restored=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(armv7a_fiq_masked(origin_psr) == armv7a_fiq_masked(current_cpsr)));
    print_stack_range_fields(sp, range);
    armv7a_platform_early_console_puts("\r\n");
}

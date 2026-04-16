#include "armv7a_handler_stack.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"

namespace {
void print_stack_fields(const Armv7aHandlerStackObservation& observation)
{
    armv7a_platform_early_console_puts(", sp=0x");
    armv7a_diag_put_hex(observation.sp);
    armv7a_platform_early_console_puts(", base=0x");
    armv7a_diag_put_hex(observation.range.base);
    armv7a_platform_early_console_puts(", top=0x");
    armv7a_diag_put_hex(observation.range.top);
    armv7a_platform_early_console_puts(", used=0x");
    armv7a_diag_put_hex(observation.used);
    armv7a_platform_early_console_puts(", in-range=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(observation.in_range));
}
} // namespace

void armv7a_print_handler_stack_evidence(const char* vector_tag, std::uint32_t current_cpsr)
{
    const auto sp = armv7a_read_sp();
    const auto range = armv7a_platform_stack_range_for_mode(current_cpsr);
    const auto observation = armv7a_make_handler_stack_observation(current_cpsr, sp, range);

    armv7a_platform_early_console_puts("ARMv7-A handler stack, vector=");
    armv7a_platform_early_console_puts(vector_tag);
    armv7a_platform_early_console_puts(", mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.current_psr));
    print_stack_fields(observation);
    armv7a_platform_early_console_puts("\r\n");
}

void armv7a_print_return_state_evidence(const char* vector_tag,
                                        std::uint32_t origin_psr,
                                        std::uint32_t current_cpsr)
{
    const auto sp = armv7a_read_sp();
    const auto range = armv7a_platform_stack_range_for_mode(current_cpsr);
    const auto observation =
        armv7a_make_return_state_observation(origin_psr, current_cpsr, sp, range);

    armv7a_platform_early_console_puts("ARMv7-A return evidence, vector=");
    armv7a_platform_early_console_puts(vector_tag);
    armv7a_platform_early_console_puts(", origin-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.origin_psr));
    armv7a_platform_early_console_puts(", current-mode=");
    armv7a_platform_early_console_puts(armv7a_mode_name(observation.current_psr));
    armv7a_platform_early_console_puts(", origin-irq=");
    armv7a_platform_early_console_puts(
        armv7a_irq_masked(observation.origin_psr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", current-irq=");
    armv7a_platform_early_console_puts(
        armv7a_irq_masked(observation.current_psr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", origin-fiq=");
    armv7a_platform_early_console_puts(
        armv7a_fiq_masked(observation.origin_psr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", current-fiq=");
    armv7a_platform_early_console_puts(
        armv7a_fiq_masked(observation.current_psr) ? "masked" : "enabled");
    armv7a_platform_early_console_puts(", mode-restored=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(observation.mode_restored));
    armv7a_platform_early_console_puts(", irq-restored=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(observation.irq_restored));
    armv7a_platform_early_console_puts(", fiq-restored=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(observation.fiq_restored));
    print_stack_fields(observation.stack);
    armv7a_platform_early_console_puts("\r\n");
}

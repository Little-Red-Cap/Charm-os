#include "armv7a_diag_context.hpp"

#include "armv7a_bringup_phase.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"

void armv7a_diag_print_context(const char* subsystem)
{
    armv7a_platform_early_console_puts("ARMv7-A diagnostic context, subsystem=");
    armv7a_platform_early_console_puts(subsystem);
    armv7a_platform_early_console_puts(", stage=");
    armv7a_platform_early_console_puts(
        armv7a_bringup_phase_name(armv7a_current_bringup_phase()));
    armv7a_platform_early_console_puts(", cpsr=0x");
    armv7a_diag_put_hex(armv7a_read_cpsr());
    armv7a_platform_early_console_puts("\r\n");
}

[[noreturn]] void armv7a_diag_report_and_halt(const char* subsystem, const char* message)
{
    armv7a_diag_print_context(subsystem);
    armv7a_platform_early_console_puts(message);
    armv7a_platform_early_console_puts("\r\n");
    armv7a_platform_idle_forever();
}

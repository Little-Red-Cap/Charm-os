#include "armv7a_cpu.hpp"
#include "armv7a_platform.hpp"

extern "C" void armv7a_run_exception_smoke_if_enabled()
{
#if defined(CHARM_ARMV7A_EXCEPTION_SMOKE_UNDEFINED)
    armv7a_platform_early_console_puts("ARMv7-A exception smoke, kind=undefined\r\n");
    armv7a_undefined_instruction();
    armv7a_platform_early_console_puts("ARMv7-A exception smoke unexpectedly returned\r\n");
    armv7a_platform_idle_forever();
#endif
}

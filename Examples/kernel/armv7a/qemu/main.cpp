#include "armv7a_bringup_phase.hpp"
#include "armv7a_boot_diagnostics.hpp"
#include "armv7a_interrupt_observation_sequence.hpp"
#include "armv7a_memory_probe_sequence.hpp"
#include "armv7a_platform.hpp"

int main()
{
#if defined(CHARM_QEMU_SEMIHOST_DEBUG)
    armv7a_platform_debug_trace("semihost: entering main\n");
#endif
    armv7a_platform_early_console_init();
    armv7a_platform_early_console_puts("Charm ARMv7-A QEMU skeleton\r\n");
    armv7a_platform_early_console_puts("Targeting Cortex-A7 first, RK3506 later.\r\n");
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kBootCpuState);
    armv7a_print_boot_cpu_state();
    armv7a_complete_bringup_phase(Armv7aBringupPhase::kBootCpuState);
    armv7a_prepare_memory_probe_environment();
    armv7a_print_memory_probe_environment();
    armv7a_activate_memory_probe_environment();
    armv7a_run_pre_dcache_probe_sequence();
    armv7a_run_post_dcache_probe_sequence();
    armv7a_run_interrupt_observation_sequence();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kIdle);
    armv7a_platform_idle_forever();
}

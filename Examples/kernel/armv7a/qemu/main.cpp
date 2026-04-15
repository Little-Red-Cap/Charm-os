#include "armv7a_attribute_probe.hpp"
#include "armv7a_boot_diagnostics.hpp"
#include "armv7a_boot_page_table.hpp"
#include "armv7a_cache.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_dcache_probe.hpp"
#include "armv7a_exception_observation.hpp"
#include "armv7a_handler_stack.hpp"
#include "armv7a_icache_probe.hpp"
#include "armv7a_interrupt_diagnostics.hpp"
#include "armv7a_interrupt_smoke.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_page_table_probe.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_section_split_probe.hpp"
#include "armv7a_small_page_probe.hpp"

extern "C" void armv7a_irq_smoke_test();
extern "C" void armv7a_sgi_smoke_test();
extern "C" void armv7a_fiq_smoke_test();
extern "C" void armv7a_prepare_abort_smoke_mappings();
extern "C" void armv7a_prepare_abort_smoke_runtime();
extern "C" void armv7a_print_abort_smoke_mapping_state();
extern "C" void armv7a_run_abort_smoke_if_enabled();
extern "C" void armv7a_run_exception_smoke_if_enabled();

int main()
{
#if defined(CHARM_QEMU_SEMIHOST_DEBUG)
    armv7a_platform_debug_trace("semihost: entering main\n");
#endif
    armv7a_platform_early_console_init();
    armv7a_platform_early_console_puts("Charm ARMv7-A QEMU skeleton\r\n");
    armv7a_platform_early_console_puts("Targeting Cortex-A7 first, RK3506 later.\r\n");
    armv7a_print_boot_cpu_state();
    armv7a_prepare_boot_identity_map();
    armv7a_prepare_abort_smoke_mappings();
    armv7a_prepare_small_page_probe_mapping();
    armv7a_prepare_attribute_probe_mapping();
    armv7a_prepare_dcache_probe_mapping();
    armv7a_prepare_icache_probe_mapping();
    armv7a_prepare_page_table_probe_mapping();
    armv7a_prepare_section_split_probe_mapping();
    armv7a_print_boot_page_table_state();
    armv7a_print_abort_smoke_mapping_state();
    armv7a_print_small_page_probe_mapping_state();
    armv7a_print_attribute_probe_mapping_state();
    armv7a_print_dcache_probe_mapping_state();
    armv7a_print_icache_probe_mapping_state();
    armv7a_print_page_table_probe_mapping_state();
    armv7a_print_section_split_probe_mapping_state();
    armv7a_enable_identity_mmu(armv7a_boot_l1_table_base());
    armv7a_prepare_abort_smoke_runtime();
    armv7a_enable_icache();
    armv7a_print_mmu_runtime_state();
    armv7a_print_charm_module_status();
    armv7a_run_small_page_probe();
    armv7a_run_attribute_probe();
    armv7a_run_icache_probe();
    armv7a_run_abort_smoke_if_enabled();
    armv7a_run_exception_smoke_if_enabled();
    armv7a_enable_dcache();
    armv7a_print_dcache_runtime_state();
    armv7a_run_dcache_probe();
    armv7a_run_page_table_probe();
    armv7a_run_section_split_probe();
    armv7a_svc_smoke_test();
    const auto svc_observation = armv7a_svc_last_observation();
    if (svc_observation.seen) {
        armv7a_print_return_state_evidence(
            "svc", svc_observation.origin_spsr, armv7a_read_cpsr());
    }
    armv7a_irq_smoke_test();
    armv7a_sgi_smoke_test();
    armv7a_fiq_smoke_test();
    armv7a_interrupt_print_security_side_evidence();
    armv7a_platform_idle_forever();
}

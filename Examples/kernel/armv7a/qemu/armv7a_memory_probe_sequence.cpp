#include "armv7a_bringup_phase.hpp"
#include "armv7a_memory_probe_sequence.hpp"

#include "armv7a_attribute_probe.hpp"
#include "armv7a_boot_diagnostics.hpp"
#include "armv7a_boot_page_table.hpp"
#include "armv7a_cache.hpp"
#include "armv7a_dcache_probe.hpp"
#include "armv7a_icache_probe.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_page_table_probe.hpp"
#include "armv7a_section_split_probe.hpp"
#include "armv7a_small_page_probe.hpp"

extern "C" void armv7a_prepare_abort_smoke_mappings();
extern "C" void armv7a_prepare_abort_smoke_runtime();
extern "C" void armv7a_print_abort_smoke_mapping_state();
extern "C" void armv7a_run_abort_smoke_if_enabled();
extern "C" void armv7a_run_exception_smoke_if_enabled();

void armv7a_prepare_memory_probe_environment()
{
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kMemoryProbePrepare);
    armv7a_prepare_boot_identity_map();
    armv7a_prepare_abort_smoke_mappings();
    armv7a_prepare_small_page_probe_mapping();
    armv7a_prepare_attribute_probe_mapping();
    armv7a_prepare_dcache_probe_mapping();
    armv7a_prepare_icache_probe_mapping();
    armv7a_prepare_page_table_probe_mapping();
    armv7a_prepare_section_split_probe_mapping();
}

void armv7a_print_memory_probe_environment()
{
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kMemoryProbeDescribe);
    armv7a_print_boot_page_table_state();
    armv7a_print_abort_smoke_mapping_state();
    armv7a_print_small_page_probe_mapping_state();
    armv7a_print_attribute_probe_mapping_state();
    armv7a_print_dcache_probe_mapping_state();
    armv7a_print_icache_probe_mapping_state();
    armv7a_print_page_table_probe_mapping_state();
    armv7a_print_section_split_probe_mapping_state();
}

void armv7a_activate_memory_probe_environment()
{
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kMmuActivate);
    armv7a_enable_identity_mmu(armv7a_boot_l1_table_base());
    armv7a_prepare_abort_smoke_runtime();
    armv7a_enable_icache();
    armv7a_print_mmu_runtime_state();
    armv7a_print_charm_module_status();
}

void armv7a_run_pre_dcache_probe_sequence()
{
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSmallPageProbe);
    armv7a_run_small_page_probe();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kAttributeProbe);
    armv7a_run_attribute_probe();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kIcacheProbe);
    armv7a_run_icache_probe();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kAbortSmoke);
    armv7a_run_abort_smoke_if_enabled();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kExceptionSmoke);
    armv7a_run_exception_smoke_if_enabled();
}

void armv7a_run_post_dcache_probe_sequence()
{
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kDcacheProbe);
    armv7a_enable_dcache();
    armv7a_print_dcache_runtime_state();
    armv7a_run_dcache_probe();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kPageTableProbe);
    armv7a_run_page_table_probe();
    armv7a_enter_bringup_phase(Armv7aBringupPhase::kSectionSplitProbe);
    armv7a_run_section_split_probe();
}

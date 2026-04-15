#pragma once

#include <cstdint>

extern "C" void armv7a_data_sync_barrier();
extern "C" void armv7a_instruction_sync_barrier();
extern "C" void armv7a_enable_irq();
extern "C" void armv7a_disable_irq();
extern "C" void armv7a_enable_fiq();
extern "C" void armv7a_disable_fiq();
extern "C" void armv7a_compiler_barrier();
extern "C" std::uint32_t armv7a_read_cpsr();
extern "C" std::uintptr_t armv7a_read_sp();
extern "C" std::uint32_t armv7a_read_id_mmfr0();
extern "C" std::uint32_t armv7a_read_id_pfr1();
extern "C" std::uint32_t armv7a_read_mpidr();
extern "C" std::uint32_t armv7a_read_sctlr();
extern "C" void armv7a_write_sctlr(std::uint32_t value);
extern "C" std::uint32_t armv7a_read_vbar();
extern "C" void armv7a_write_vbar(std::uint32_t value);
extern "C" void armv7a_branch_to_address(std::uintptr_t target);
extern "C" std::uint32_t armv7a_load_word_relaxed(std::uintptr_t address);
extern "C" void armv7a_undefined_instruction();
extern "C" void armv7a_svc_smoke_test();

std::uint32_t armv7a_id_mmfr0_vmsa_field(std::uint32_t value);
std::uint32_t armv7a_id_mmfr0_pmsa_field(std::uint32_t value);
std::uint32_t armv7a_id_pfr1_security_field(std::uint32_t value);
std::uint32_t armv7a_id_pfr1_virtualization_field(std::uint32_t value);
std::uint32_t armv7a_id_pfr1_gentimer_field(std::uint32_t value);
const char* armv7a_feature_presence_name(std::uint32_t field);
bool armv7a_irq_masked(std::uint32_t psr);
bool armv7a_fiq_masked(std::uint32_t psr);
bool armv7a_alignment_check_enabled(std::uint32_t sctlr);
const char* armv7a_mode_name(std::uint32_t psr);

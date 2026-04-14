#pragma once

#include <cstdint>

extern "C" std::uint32_t armv7a_read_ttbr0();
extern "C" std::uint32_t armv7a_read_ttbr1();
extern "C" std::uint32_t armv7a_read_ttbcr();
extern "C" std::uint32_t armv7a_read_dacr();
extern "C" void armv7a_write_ttbr0(std::uint32_t value);
extern "C" void armv7a_write_ttbcr(std::uint32_t value);
extern "C" void armv7a_write_dacr(std::uint32_t value);
extern "C" std::uint32_t armv7a_read_dfsr();
extern "C" std::uint32_t armv7a_read_ifsr();
extern "C" std::uint32_t armv7a_read_adfsr();
extern "C" std::uint32_t armv7a_read_aifsr();
extern "C" std::uint32_t armv7a_read_dfar();
extern "C" std::uint32_t armv7a_read_ifar();
extern "C" void armv7a_invalidate_tlb_all();
extern "C" void armv7a_invalidate_tlb_mva(std::uintptr_t virtual_address);
extern "C" void armv7a_invalidate_icache_all();
extern "C" void armv7a_invalidate_branch_predictor();

std::uint32_t armv7a_build_ttbr0(std::uintptr_t table_base);
std::uint32_t armv7a_early_dacr_value();
void armv7a_enable_identity_mmu(std::uintptr_t table_base);
void armv7a_enable_icache();
void armv7a_sync_tlb_mapping_change(std::uintptr_t descriptor_address,
                                    std::uintptr_t virtual_address);
void armv7a_sync_instruction_mapping_change(std::uintptr_t descriptor_address,
                                            std::uintptr_t virtual_address);

bool armv7a_mmu_enabled(std::uint32_t sctlr);
bool armv7a_dcache_enabled(std::uint32_t sctlr);
bool armv7a_icache_enabled(std::uint32_t sctlr);
bool armv7a_high_vectors_enabled(std::uint32_t sctlr);

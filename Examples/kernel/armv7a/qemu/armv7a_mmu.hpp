#pragma once

#include <cstdint>

extern "C" std::uint32_t armv7a_read_ttbr0();
extern "C" std::uint32_t armv7a_read_ttbr1();
extern "C" std::uint32_t armv7a_read_ttbcr();
extern "C" std::uint32_t armv7a_read_dacr();
extern "C" std::uint32_t armv7a_read_dfsr();
extern "C" std::uint32_t armv7a_read_ifsr();
extern "C" std::uint32_t armv7a_read_adfsr();
extern "C" std::uint32_t armv7a_read_aifsr();
extern "C" std::uint32_t armv7a_read_dfar();
extern "C" std::uint32_t armv7a_read_ifar();

bool armv7a_mmu_enabled(std::uint32_t sctlr);
bool armv7a_dcache_enabled(std::uint32_t sctlr);
bool armv7a_icache_enabled(std::uint32_t sctlr);
bool armv7a_high_vectors_enabled(std::uint32_t sctlr);

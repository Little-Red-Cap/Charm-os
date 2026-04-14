#include "armv7a_mmu.hpp"

#include "armv7a_cpu.hpp"

namespace {
constexpr std::uint32_t kSctlrM = 1u << 0;
constexpr std::uint32_t kSctlrC = 1u << 2;
constexpr std::uint32_t kSctlrI = 1u << 12;
constexpr std::uint32_t kSctlrV = 1u << 13;
constexpr std::uint32_t kTtbr0BaseMask = 0xffffc000u;
constexpr std::uint32_t kDomain0Manager = 0x3u;
} // namespace

extern "C" std::uint32_t armv7a_read_ttbr0()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_ttbr1()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c2, c0, 1" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_ttbcr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c2, c0, 2" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_dacr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(value));
    return value;
}

extern "C" void armv7a_write_ttbr0(std::uint32_t value)
{
    asm volatile("mcr p15, 0, %0, c2, c0, 0" : : "r"(value) : "memory");
    armv7a_instruction_sync_barrier();
}

extern "C" void armv7a_write_ttbcr(std::uint32_t value)
{
    asm volatile("mcr p15, 0, %0, c2, c0, 2" : : "r"(value) : "memory");
    armv7a_instruction_sync_barrier();
}

extern "C" void armv7a_write_dacr(std::uint32_t value)
{
    asm volatile("mcr p15, 0, %0, c3, c0, 0" : : "r"(value) : "memory");
    armv7a_instruction_sync_barrier();
}

extern "C" std::uint32_t armv7a_read_dfsr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_ifsr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c5, c0, 1" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_adfsr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c5, c1, 0" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_aifsr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c5, c1, 1" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_dfar()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_ifar()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c6, c0, 2" : "=r"(value));
    return value;
}

extern "C" void armv7a_invalidate_tlb_all()
{
    std::uint32_t zero = 0;
    asm volatile("mcr p15, 0, %0, c8, c7, 0" : : "r"(zero) : "memory");
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

extern "C" void armv7a_invalidate_icache_all()
{
    std::uint32_t zero = 0;
    asm volatile("mcr p15, 0, %0, c7, c5, 0" : : "r"(zero) : "memory");
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

extern "C" void armv7a_invalidate_branch_predictor()
{
    std::uint32_t zero = 0;
    asm volatile("mcr p15, 0, %0, c7, c5, 6" : : "r"(zero) : "memory");
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

std::uint32_t armv7a_build_ttbr0(std::uintptr_t table_base)
{
    return static_cast<std::uint32_t>(table_base) & kTtbr0BaseMask;
}

std::uint32_t armv7a_early_dacr_value()
{
    return kDomain0Manager;
}

void armv7a_enable_identity_mmu(std::uintptr_t table_base)
{
    auto sctlr = armv7a_read_sctlr();
    if ((sctlr & kSctlrM) != 0u) {
        return;
    }

    armv7a_invalidate_tlb_all();
    armv7a_invalidate_icache_all();
    armv7a_invalidate_branch_predictor();

    armv7a_write_ttbcr(0u);
    armv7a_write_ttbr0(armv7a_build_ttbr0(table_base));
    armv7a_write_dacr(armv7a_early_dacr_value());

    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();

    sctlr |= kSctlrM;
    asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr) : "memory");
    armv7a_instruction_sync_barrier();
}

bool armv7a_mmu_enabled(std::uint32_t sctlr)
{
    return (sctlr & kSctlrM) != 0u;
}

bool armv7a_dcache_enabled(std::uint32_t sctlr)
{
    return (sctlr & kSctlrC) != 0u;
}

bool armv7a_icache_enabled(std::uint32_t sctlr)
{
    return (sctlr & kSctlrI) != 0u;
}

bool armv7a_high_vectors_enabled(std::uint32_t sctlr)
{
    return (sctlr & kSctlrV) != 0u;
}

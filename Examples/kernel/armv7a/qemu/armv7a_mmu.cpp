#include "armv7a_mmu.hpp"

namespace {
constexpr std::uint32_t kSctlrM = 1u << 0;
constexpr std::uint32_t kSctlrC = 1u << 2;
constexpr std::uint32_t kSctlrI = 1u << 12;
constexpr std::uint32_t kSctlrV = 1u << 13;
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

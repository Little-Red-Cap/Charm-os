#include "armv7a_cache.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

#include "armv7a_cpu.hpp"

extern "C" std::uint32_t armv7a_read_sctlr();

namespace {
constexpr std::uint32_t kSctlrC = 1u << 2;
constexpr std::uint32_t kClidrLocShift = 24u;
constexpr std::uint32_t kClidrLocMask = 0x7u;
constexpr std::uint32_t kClidrLevelTypeMask = 0x7u;
constexpr std::uint32_t kCcsidrLineSizeMask = 0x7u;
constexpr std::uint32_t kCcsidrWaysMask = 0x3ffu;
constexpr std::uint32_t kCcsidrWaysShift = 3u;
constexpr std::uint32_t kCcsidrSetsMask = 0x7fffu;
constexpr std::uint32_t kCcsidrSetsShift = 13u;

std::uint32_t armv7a_read_clidr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 1, %0, c0, c0, 1" : "=r"(value));
    return value;
}

void armv7a_write_csselr(std::uint32_t value)
{
    asm volatile("mcr p15, 2, %0, c0, c0, 0" : : "r"(value) : "memory");
    armv7a_instruction_sync_barrier();
}

std::uint32_t armv7a_read_ccsidr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 1, %0, c0, c0, 0" : "=r"(value));
    return value;
}

void armv7a_invalidate_dcache_by_set_way(std::uint32_t value)
{
    asm volatile("mcr p15, 0, %0, c7, c6, 2" : : "r"(value) : "memory");
}

void armv7a_invalidate_dcache_line_mva(std::uintptr_t virtual_address)
{
    const auto mva = static_cast<std::uint32_t>(virtual_address);
    asm volatile("mcr p15, 0, %0, c7, c6, 1" : : "r"(mva) : "memory");
}

void armv7a_clean_invalidate_dcache_line_mva(std::uintptr_t virtual_address)
{
    const auto mva = static_cast<std::uint32_t>(virtual_address);
    asm volatile("mcr p15, 0, %0, c7, c14, 1" : : "r"(mva) : "memory");
}

std::uint32_t armv7a_l1_cache_type(std::uint32_t clidr)
{
    return clidr & kClidrLevelTypeMask;
}

std::uint32_t armv7a_line_size_bytes(std::uint32_t ccsidr)
{
    return 1u << ((ccsidr & kCcsidrLineSizeMask) + 4u);
}

std::uint32_t armv7a_line_aligned_start(std::uintptr_t start, std::uint32_t line_size)
{
    return static_cast<std::uint32_t>(start) & ~(line_size - 1u);
}

std::uint32_t armv7a_line_aligned_end(std::uintptr_t start,
                                      std::size_t size,
                                      std::uint32_t line_size)
{
    const auto end = static_cast<std::uint32_t>(start + size);
    return (end + line_size - 1u) & ~(line_size - 1u);
}
} // namespace

Armv7aL1DataCacheGeometry armv7a_read_l1_dcache_geometry()
{
    const auto clidr = armv7a_read_clidr();
    const auto cache_type = armv7a_l1_cache_type(clidr);
    if (cache_type < 2u) {
        return Armv7aL1DataCacheGeometry{
            .clidr = clidr,
            .ccsidr = 0u,
            .line_size_bytes = 0u,
            .ways = 0u,
            .sets = 0u,
            .present = false,
        };
    }

    armv7a_write_csselr(0u);
    const auto ccsidr = armv7a_read_ccsidr();
    return Armv7aL1DataCacheGeometry{
        .clidr = clidr,
        .ccsidr = ccsidr,
        .line_size_bytes = armv7a_line_size_bytes(ccsidr),
        .ways = ((ccsidr >> kCcsidrWaysShift) & kCcsidrWaysMask) + 1u,
        .sets = ((ccsidr >> kCcsidrSetsShift) & kCcsidrSetsMask) + 1u,
        .present = true,
    };
}

void armv7a_invalidate_dcache_all()
{
    const auto clidr = armv7a_read_clidr();
    const auto loc = (clidr >> kClidrLocShift) & kClidrLocMask;
    if (loc == 0u) {
        return;
    }

    for (std::uint32_t level = 0; level < loc; ++level) {
        const auto cache_type = (clidr >> (level * 3u)) & kClidrLevelTypeMask;
        if (cache_type < 2u) {
            continue;
        }

        const auto level_selector = level << 1u;
        armv7a_write_csselr(level_selector);
        const auto ccsidr = armv7a_read_ccsidr();
        const auto line_len_shift = (ccsidr & kCcsidrLineSizeMask) + 4u;
        const auto max_way = (ccsidr >> kCcsidrWaysShift) & kCcsidrWaysMask;
        const auto max_set = (ccsidr >> kCcsidrSetsShift) & kCcsidrSetsMask;
        const auto way_shift = max_way == 0u ? 32u : std::countl_zero(max_way);

        for (std::int32_t way = static_cast<std::int32_t>(max_way); way >= 0; --way) {
            for (std::int32_t set = static_cast<std::int32_t>(max_set); set >= 0; --set) {
                auto operand = level_selector |
                               (static_cast<std::uint32_t>(set) << line_len_shift);
                if (way_shift < 32u) {
                    operand |= static_cast<std::uint32_t>(way) << way_shift;
                }
                armv7a_invalidate_dcache_by_set_way(operand);
            }
        }
    }

    armv7a_write_csselr(0u);
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

void armv7a_invalidate_dcache_range(std::uintptr_t start, std::uint32_t size_bytes)
{
    if (size_bytes == 0u) {
        return;
    }

    const auto geometry = armv7a_read_l1_dcache_geometry();
    if (!geometry.present || geometry.line_size_bytes == 0u) {
        return;
    }

    for (auto mva = armv7a_line_aligned_start(start, geometry.line_size_bytes);
         mva < armv7a_line_aligned_end(start, size_bytes, geometry.line_size_bytes);
         mva += geometry.line_size_bytes) {
        armv7a_invalidate_dcache_line_mva(mva);
    }

    armv7a_data_sync_barrier();
}

void armv7a_clean_invalidate_dcache_range(std::uintptr_t start, std::uint32_t size_bytes)
{
    if (size_bytes == 0u) {
        return;
    }

    const auto geometry = armv7a_read_l1_dcache_geometry();
    if (!geometry.present || geometry.line_size_bytes == 0u) {
        return;
    }

    for (auto mva = armv7a_line_aligned_start(start, geometry.line_size_bytes);
         mva < armv7a_line_aligned_end(start, size_bytes, geometry.line_size_bytes);
         mva += geometry.line_size_bytes) {
        armv7a_clean_invalidate_dcache_line_mva(mva);
    }

    armv7a_data_sync_barrier();
}

void armv7a_enable_dcache()
{
    auto sctlr = armv7a_read_sctlr();
    if ((sctlr & kSctlrC) != 0u) {
        return;
    }

    armv7a_invalidate_dcache_all();
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();

    sctlr |= kSctlrC;
    asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(sctlr) : "memory");
    armv7a_instruction_sync_barrier();
}

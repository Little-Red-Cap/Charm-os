#pragma once

#include <cstdint>

struct Armv7aL1DataCacheGeometry {
    std::uint32_t clidr;
    std::uint32_t ccsidr;
    std::uint32_t line_size_bytes;
    std::uint32_t ways;
    std::uint32_t sets;
    bool present;
};

Armv7aL1DataCacheGeometry armv7a_read_l1_dcache_geometry();
void armv7a_invalidate_dcache_all();
void armv7a_invalidate_dcache_range(std::uintptr_t start, std::uint32_t size_bytes);
void armv7a_clean_invalidate_dcache_range(std::uintptr_t start, std::uint32_t size_bytes);
void armv7a_enable_dcache();

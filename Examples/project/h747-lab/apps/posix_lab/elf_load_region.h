#pragma once

#include <cstddef>
#include <cstdint>

namespace h747::apps::posix_lab {

inline constexpr std::size_t kElfLoadRegionSize = 0x2000u;

extern "C" std::byte __elf_load_start__[];
extern "C" std::byte __elf_load_end__[];

void prepare_elf_load_region() noexcept;

inline std::byte* elf_load_region_base() noexcept {
    return __elf_load_start__;
}

inline std::size_t elf_load_region_capacity() noexcept {
    return static_cast<std::size_t>(__elf_load_end__ - __elf_load_start__);
}

} // namespace h747::apps::posix_lab

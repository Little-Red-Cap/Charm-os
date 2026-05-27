#include "elf_load_region.h"

#include "stm32h7xx.h"

namespace h747::apps::app_lab {
namespace {

constexpr std::uintptr_t cache_align_down(const std::uintptr_t address) noexcept {
    return address & ~static_cast<std::uintptr_t>(31u);
}

constexpr std::uint32_t cache_aligned_length(const std::uintptr_t address,
                                             const std::uint32_t length) noexcept {
    const std::uintptr_t start = cache_align_down(address);
    const std::uintptr_t end = (address + length + 31u) & ~static_cast<std::uintptr_t>(31u);
    return static_cast<std::uint32_t>(end - start);
}

} // namespace

void prepare_elf_load_region() noexcept {
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if ((SCB->CCR & SCB_CCR_DC_Msk) != 0U) {
        const auto addr = reinterpret_cast<std::uintptr_t>(elf_load_region_base());
        auto* aligned = reinterpret_cast<std::uint32_t*>(cache_align_down(addr));
        const auto bytes = cache_aligned_length(addr, static_cast<std::uint32_t>(elf_load_region_capacity()));
        SCB_CleanDCache_by_Addr(aligned, static_cast<std::int32_t>(bytes));
        SCB_InvalidateDCache_by_Addr(aligned, static_cast<std::int32_t>(bytes));
    }
#endif
#if defined(__ICACHE_PRESENT) && (__ICACHE_PRESENT == 1U)
    SCB_InvalidateICache();
#endif
    __DSB();
    __ISB();
}

} // namespace h747::apps::app_lab

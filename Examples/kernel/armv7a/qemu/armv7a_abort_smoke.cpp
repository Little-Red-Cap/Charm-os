#include <cstdint>

#include "armv7a_boot_page_table.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_mmu.hpp"

extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
constexpr std::uintptr_t kAbortSmokeAddress = 0x20000000u;
constexpr std::uintptr_t kAbortSmokeXnAliasBase = 0x50000000u;
constexpr std::uintptr_t kAbortSmokeDataPermAliasBase = 0x51000000u;
constexpr std::uintptr_t kAbortSmokeDataPageAliasAddress = 0x53000040u;
constexpr std::uintptr_t kAbortSmokeDataPagePermAliasBase = 0x54000000u;
constexpr std::uintptr_t kAbortSmokePrefetchPageXnAliasBase = 0x55000000u;
constexpr std::uintptr_t kAbortSmokePrefetchPageAliasBase = 0x56000000u;
constexpr std::uintptr_t kAbortSmokeDataPagePermRuntimeAliasBase = 0x57000000u;
constexpr std::uintptr_t kAbortSmokePrefetchPageXnRuntimeAliasBase = 0x58000000u;
constexpr std::uintptr_t kSectionSize = 1u << 20;
constexpr std::uintptr_t kSectionMask = ~(kSectionSize - 1u);
constexpr std::uintptr_t kSectionOffsetMask = kSectionSize - 1u;
constexpr std::uintptr_t kSmallPageSize = 1u << 12;
constexpr std::uintptr_t kSmallPageMask = ~(kSmallPageSize - 1u);
constexpr std::uintptr_t kSmallPageOffsetMask = kSmallPageSize - 1u;
constexpr std::uint32_t kAbortSmokeClientDomain = 1u;
constexpr std::uint32_t kAbortSmokeClientDacr = 0x7u;
constexpr std::uint32_t kAbortSmokeExecProbeReturnValue = 0x00000043u;
constexpr std::uint32_t kAbortSmokeDataWriteValue = 0xA5A55A5Au;
constexpr std::uint32_t kAbortSmokeRuntimeDataWriteValue = 0x5AA55AA5u;

void early_uart_write_hex32(std::uint32_t value)
{
    constexpr char kHex[] = "0123456789ABCDEF";
    char buffer[9]{};
    for (int i = 0; i < 8; ++i) {
        const auto shift = 28 - (i * 4);
        buffer[i] = kHex[(value >> shift) & 0x0fu];
    }
    early_uart_puts(buffer);
}

extern "C" [[gnu::noinline]] void armv7a_abort_xn_target()
{
    asm volatile("" ::: "memory");
}

extern "C" [[gnu::noinline]] std::uint32_t armv7a_abort_exec_probe_target()
{
    asm volatile("" ::: "memory");
    return kAbortSmokeExecProbeReturnValue;
}

volatile std::uint32_t g_armv7a_abort_data_perm_target = 0x13579BDFu;
volatile std::uint32_t g_armv7a_abort_data_page_perm_target = 0x2468ACE0u;
volatile std::uint32_t g_armv7a_abort_data_page_perm_runtime_target = 0x0BADCAFEu;

std::uintptr_t armv7a_abort_xn_target_address()
{
    return reinterpret_cast<std::uintptr_t>(&armv7a_abort_xn_target);
}

std::uintptr_t armv7a_abort_xn_alias_address()
{
    const auto target = armv7a_abort_xn_target_address();
    return kAbortSmokeXnAliasBase + (target & kSectionOffsetMask);
}

std::uintptr_t armv7a_abort_prefetch_page_xn_alias_address()
{
    const auto target = armv7a_abort_xn_target_address();
    return kAbortSmokePrefetchPageXnAliasBase + (target & kSmallPageOffsetMask);
}

std::uintptr_t armv7a_abort_prefetch_page_alias_address()
{
    const auto target = armv7a_abort_xn_target_address();
    return kAbortSmokePrefetchPageAliasBase + (target & kSmallPageOffsetMask);
}

std::uintptr_t armv7a_abort_exec_probe_target_address()
{
    return reinterpret_cast<std::uintptr_t>(&armv7a_abort_exec_probe_target);
}

std::uintptr_t armv7a_abort_prefetch_page_xn_runtime_alias_address()
{
    const auto target = armv7a_abort_exec_probe_target_address();
    return kAbortSmokePrefetchPageXnRuntimeAliasBase + (target & kSmallPageOffsetMask);
}

std::uintptr_t armv7a_abort_data_perm_target_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_abort_data_perm_target);
}

std::uintptr_t armv7a_abort_data_perm_alias_address()
{
    const auto target = armv7a_abort_data_perm_target_address();
    return kAbortSmokeDataPermAliasBase + (target & kSectionOffsetMask);
}

std::uintptr_t armv7a_abort_data_page_perm_target_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_abort_data_page_perm_target);
}

std::uintptr_t armv7a_abort_data_page_perm_alias_address()
{
    const auto target = armv7a_abort_data_page_perm_target_address();
    return kAbortSmokeDataPagePermAliasBase + (target & kSmallPageOffsetMask);
}

std::uintptr_t armv7a_abort_data_page_perm_runtime_target_address()
{
    return reinterpret_cast<std::uintptr_t>(&g_armv7a_abort_data_page_perm_runtime_target);
}

std::uintptr_t armv7a_abort_data_page_perm_runtime_alias_address()
{
    const auto target = armv7a_abort_data_page_perm_runtime_target_address();
    return kAbortSmokeDataPagePermRuntimeAliasBase + (target & kSmallPageOffsetMask);
}
} // namespace

extern "C" void armv7a_prepare_abort_smoke_mappings()
{
#if defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_XN)
    const auto target = armv7a_abort_xn_target_address();
    armv7a_boot_l1_map_section(kAbortSmokeXnAliasBase,
                               target & kSectionMask,
                               Armv7aBootSectionType::kNormalExecuteNever,
                               kAbortSmokeClientDomain);
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE)
    armv7a_boot_l2_prepare_table(armv7a_abort_prefetch_page_alias_address());
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN)
    const auto target = armv7a_abort_xn_target_address();
    armv7a_boot_l2_map_small_page(kAbortSmokePrefetchPageXnAliasBase,
                                  target & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecuteNever,
                                  kAbortSmokeClientDomain);
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN_RUNTIME)
    const auto target = armv7a_abort_exec_probe_target_address();
    armv7a_boot_l2_map_small_page(kAbortSmokePrefetchPageXnRuntimeAliasBase,
                                  target & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecutable,
                                  kAbortSmokeClientDomain);
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PERM)
    const auto target = armv7a_abort_data_perm_target_address();
    armv7a_boot_l1_map_section(kAbortSmokeDataPermAliasBase,
                               target & kSectionMask,
                               Armv7aBootSectionType::kNormalNoAccessExecuteNever,
                               kAbortSmokeClientDomain);
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE)
    armv7a_boot_l2_prepare_table(kAbortSmokeDataPageAliasAddress);
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM)
    const auto target = armv7a_abort_data_page_perm_target_address();
    armv7a_boot_l2_map_small_page(kAbortSmokeDataPagePermAliasBase,
                                  target & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalNoAccessExecuteNever,
                                  kAbortSmokeClientDomain);
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM_RUNTIME)
    const auto target = armv7a_abort_data_page_perm_runtime_target_address();
    armv7a_boot_l2_map_small_page(kAbortSmokeDataPagePermRuntimeAliasBase,
                                  target & kSmallPageMask,
                                  Armv7aBootSmallPageType::kNormalExecuteNever,
                                  kAbortSmokeClientDomain);
#endif
}

extern "C" void armv7a_prepare_abort_smoke_runtime()
{
#if defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_XN) || \
    defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN) || \
    defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN_RUNTIME) || \
    defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PERM) || \
    defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM) || \
    defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM_RUNTIME)
    armv7a_write_dacr(kAbortSmokeClientDacr);
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
#endif
}

extern "C" void armv7a_print_abort_smoke_mapping_state()
{
#if defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_XN)
    early_uart_puts("ARMv7-A XN alias ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_xn_alias_address()));
    early_uart_puts(", pa=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_xn_target_address()));
    early_uart_puts(", desc=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(kAbortSmokeXnAliasBase));
    early_uart_puts("\r\n");
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE)
    early_uart_puts("ARMv7-A prefetch-page alias ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_prefetch_page_alias_address()));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(armv7a_abort_prefetch_page_alias_address()));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(armv7a_abort_prefetch_page_alias_address()));
    early_uart_puts("\r\n");
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN)
    early_uart_puts("ARMv7-A page-XN alias ready, va=0x");
    early_uart_write_hex32(
        static_cast<std::uint32_t>(armv7a_abort_prefetch_page_xn_alias_address()));
    early_uart_puts(", pa=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_xn_target_address()));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(
        armv7a_boot_l1_descriptor(armv7a_abort_prefetch_page_xn_alias_address()));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(
        armv7a_boot_l2_descriptor(armv7a_abort_prefetch_page_xn_alias_address()));
    early_uart_puts("\r\n");
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN_RUNTIME)
    early_uart_puts("ARMv7-A runtime page-XN alias ready, va=0x");
    early_uart_write_hex32(
        static_cast<std::uint32_t>(armv7a_abort_prefetch_page_xn_runtime_alias_address()));
    early_uart_puts(", pa=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_exec_probe_target_address()));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(
        armv7a_boot_l1_descriptor(armv7a_abort_prefetch_page_xn_runtime_alias_address()));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(
        armv7a_boot_l2_descriptor(armv7a_abort_prefetch_page_xn_runtime_alias_address()));
    early_uart_puts("\r\n");
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PERM)
    early_uart_puts("ARMv7-A data alias ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_data_perm_alias_address()));
    early_uart_puts(", pa=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_data_perm_target_address()));
    early_uart_puts(", desc=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(kAbortSmokeDataPermAliasBase));
    early_uart_puts("\r\n");
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE)
    early_uart_puts("ARMv7-A data-page alias ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(kAbortSmokeDataPageAliasAddress));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(kAbortSmokeDataPageAliasAddress));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(kAbortSmokeDataPageAliasAddress));
    early_uart_puts("\r\n");
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM)
    early_uart_puts("ARMv7-A data-page-perm alias ready, va=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_data_page_perm_alias_address()));
    early_uart_puts(", pa=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_data_page_perm_target_address()));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(armv7a_boot_l1_descriptor(armv7a_abort_data_page_perm_alias_address()));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(armv7a_boot_l2_descriptor(armv7a_abort_data_page_perm_alias_address()));
    early_uart_puts("\r\n");
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM_RUNTIME)
    early_uart_puts("ARMv7-A runtime data-page alias ready, va=0x");
    early_uart_write_hex32(
        static_cast<std::uint32_t>(armv7a_abort_data_page_perm_runtime_alias_address()));
    early_uart_puts(", pa=0x");
    early_uart_write_hex32(
        static_cast<std::uint32_t>(armv7a_abort_data_page_perm_runtime_target_address()));
    early_uart_puts(", l1=0x");
    early_uart_write_hex32(
        armv7a_boot_l1_descriptor(armv7a_abort_data_page_perm_runtime_alias_address()));
    early_uart_puts(", l2=0x");
    early_uart_write_hex32(
        armv7a_boot_l2_descriptor(armv7a_abort_data_page_perm_runtime_alias_address()));
    early_uart_puts("\r\n");
#endif
}

extern "C" void armv7a_run_abort_smoke_if_enabled()
{
#if defined(CHARM_ARMV7A_ABORT_SMOKE_DATA)
    early_uart_puts("ARMv7-A abort smoke, kind=data, addr=0x20000000\r\n");
    auto* const probe = reinterpret_cast<volatile std::uint32_t*>(kAbortSmokeAddress);
    const auto value = *probe;
    static_cast<void>(value);
    early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
    charm_spin();
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH)
    early_uart_puts("ARMv7-A abort smoke, kind=prefetch, addr=0x20000000\r\n");
    const auto target = static_cast<std::uint32_t>(kAbortSmokeAddress);
    asm volatile("bx %0" : : "r"(target) : "memory");
    early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
    charm_spin();
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_XN)
    early_uart_puts("ARMv7-A abort smoke, kind=prefetch-xn, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_xn_alias_address()));
    early_uart_puts("\r\n");
    const auto target = reinterpret_cast<void (*)()>(armv7a_abort_xn_alias_address());
    target();
    early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
    charm_spin();
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE)
    early_uart_puts("ARMv7-A abort smoke, kind=prefetch-page, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_prefetch_page_alias_address()));
    early_uart_puts("\r\n");
    const auto target = reinterpret_cast<void (*)()>(armv7a_abort_prefetch_page_alias_address());
    target();
    early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
    charm_spin();
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN)
    early_uart_puts("ARMv7-A abort smoke, kind=prefetch-page-xn, addr=0x");
    early_uart_write_hex32(
        static_cast<std::uint32_t>(armv7a_abort_prefetch_page_xn_alias_address()));
    early_uart_puts("\r\n");
    const auto target =
        reinterpret_cast<void (*)()>(armv7a_abort_prefetch_page_xn_alias_address());
    target();
    early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
    charm_spin();
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_PREFETCH_PAGE_XN_RUNTIME)
    {
        const auto alias_address = armv7a_abort_prefetch_page_xn_runtime_alias_address();
        auto* const probe = reinterpret_cast<std::uint32_t (*)()>(alias_address);
        const auto returned = probe();
        early_uart_puts("ARMv7-A runtime page-XN probe, addr=0x");
        early_uart_write_hex32(static_cast<std::uint32_t>(alias_address));
        early_uart_puts(", return=0x");
        early_uart_write_hex32(returned);
        early_uart_puts("\r\n");

        const auto target = armv7a_abort_exec_probe_target_address();
        armv7a_boot_l2_map_small_page(kAbortSmokePrefetchPageXnRuntimeAliasBase,
                                      target & kSmallPageMask,
                                      Armv7aBootSmallPageType::kNormalExecuteNever,
                                      kAbortSmokeClientDomain);
        armv7a_sync_instruction_mapping_change(alias_address);

        early_uart_puts("ARMv7-A runtime page-XN flip, addr=0x");
        early_uart_write_hex32(static_cast<std::uint32_t>(alias_address));
        early_uart_puts(", l2=0x");
        early_uart_write_hex32(armv7a_boot_l2_descriptor(alias_address));
        early_uart_puts("\r\n");

        early_uart_puts("ARMv7-A abort smoke, kind=prefetch-page-xn-runtime, addr=0x");
        early_uart_write_hex32(static_cast<std::uint32_t>(alias_address));
        early_uart_puts("\r\n");
        const auto target_fn = reinterpret_cast<void (*)()>(alias_address);
        target_fn();
        early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
        charm_spin();
    }
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PERM)
    early_uart_puts("ARMv7-A abort smoke, kind=data-perm, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_data_perm_alias_address()));
    early_uart_puts(", value=0x");
    early_uart_write_hex32(kAbortSmokeDataWriteValue);
    early_uart_puts("\r\n");
    auto* const target =
        reinterpret_cast<volatile std::uint32_t*>(armv7a_abort_data_perm_alias_address());
    *target = kAbortSmokeDataWriteValue;
    early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
    charm_spin();
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE)
    early_uart_puts("ARMv7-A abort smoke, kind=data-page, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(kAbortSmokeDataPageAliasAddress));
    early_uart_puts("\r\n");
    auto* const probe = reinterpret_cast<volatile std::uint32_t*>(kAbortSmokeDataPageAliasAddress);
    const auto value = *probe;
    static_cast<void>(value);
    early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
    charm_spin();
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM)
    early_uart_puts("ARMv7-A abort smoke, kind=data-page-perm, addr=0x");
    early_uart_write_hex32(static_cast<std::uint32_t>(armv7a_abort_data_page_perm_alias_address()));
    early_uart_puts(", value=0x");
    early_uart_write_hex32(kAbortSmokeDataWriteValue);
    early_uart_puts("\r\n");
    auto* const target =
        reinterpret_cast<volatile std::uint32_t*>(armv7a_abort_data_page_perm_alias_address());
    *target = kAbortSmokeDataWriteValue;
    early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
    charm_spin();
#elif defined(CHARM_ARMV7A_ABORT_SMOKE_DATA_PAGE_PERM_RUNTIME)
    {
        const auto alias_address = armv7a_abort_data_page_perm_runtime_alias_address();
        auto* const target = reinterpret_cast<volatile std::uint32_t*>(alias_address);
        const auto before = *target;
        *target = kAbortSmokeRuntimeDataWriteValue;
        armv7a_data_sync_barrier();
        const auto direct = g_armv7a_abort_data_page_perm_runtime_target;

        early_uart_puts("ARMv7-A runtime data-page probe, addr=0x");
        early_uart_write_hex32(static_cast<std::uint32_t>(alias_address));
        early_uart_puts(", before=0x");
        early_uart_write_hex32(before);
        early_uart_puts(", after=0x");
        early_uart_write_hex32(kAbortSmokeRuntimeDataWriteValue);
        early_uart_puts(", direct=0x");
        early_uart_write_hex32(direct);
        early_uart_puts("\r\n");

        const auto target_address = armv7a_abort_data_page_perm_runtime_target_address();
        armv7a_boot_l2_map_small_page(kAbortSmokeDataPagePermRuntimeAliasBase,
                                      target_address & kSmallPageMask,
                                      Armv7aBootSmallPageType::kNormalNoAccessExecuteNever,
                                      kAbortSmokeClientDomain);
        armv7a_sync_tlb_mapping_change(alias_address);

        early_uart_puts("ARMv7-A runtime data-page flip, addr=0x");
        early_uart_write_hex32(static_cast<std::uint32_t>(alias_address));
        early_uart_puts(", l2=0x");
        early_uart_write_hex32(armv7a_boot_l2_descriptor(alias_address));
        early_uart_puts("\r\n");

        early_uart_puts("ARMv7-A abort smoke, kind=data-page-perm-runtime, addr=0x");
        early_uart_write_hex32(static_cast<std::uint32_t>(alias_address));
        early_uart_puts(", value=0x");
        early_uart_write_hex32(kAbortSmokeDataWriteValue);
        early_uart_puts("\r\n");
        *target = kAbortSmokeDataWriteValue;
        early_uart_puts("ARMv7-A abort smoke unexpectedly returned\r\n");
        charm_spin();
    }
#else
    // Keep the default smoke path stable unless a preset explicitly opts in.
#endif
}

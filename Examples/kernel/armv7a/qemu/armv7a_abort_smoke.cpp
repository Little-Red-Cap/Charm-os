#include <cstdint>

extern "C" void early_uart_puts(const char* text);
extern "C" [[noreturn]] void charm_spin();

namespace {
constexpr std::uintptr_t kAbortSmokeAddress = 0x20000000u;
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
#else
    // Keep the default smoke path stable unless a preset explicitly opts in.
#endif
}

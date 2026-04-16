#include "armv7a_cpu.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

extern "C" {
extern char __und_stack_base;
extern char __und_stack_top;
extern char __abt_stack_base;
extern char __abt_stack_top;
extern char __irq_stack_base;
extern char __irq_stack_top;
extern char __fiq_stack_base;
extern char __fiq_stack_top;
extern char __svc_stack_base;
extern char __svc_stack_top;
extern char __sys_stack_base;
extern char __sys_stack_top;
}

namespace {
constexpr std::uint32_t kPsrModeMask = 0x1fu;
constexpr std::uint32_t kModeFiq = 0x11u;
constexpr std::uint32_t kModeIrq = 0x12u;
constexpr std::uint32_t kModeSvc = 0x13u;
constexpr std::uint32_t kModeAbt = 0x17u;
constexpr std::uint32_t kModeUnd = 0x1bu;
constexpr std::uint32_t kModeSys = 0x1fu;

constexpr Armv7aPlatformAddressSpace kQemuVirtAddressSpace{
    0x40000000u,
    64u * 1024u * 1024u,
    0x40200000u,
};

constexpr Armv7aPlatformMmioLayout kQemuVirtMmioLayout{
    0x09000000u,
    0x08000000u,
    0x08010000u,
};

constexpr Armv7aPlatformProbeLayout kQemuVirtProbeLayout{
    0x52000000u,
    0x52100000u,
    0x52200000u,
    0x52300000u,
    0x52400000u,
    0x52500000u,
    0x52600000u,
    0x20000000u,
    0x50000000u,
    0x51000000u,
    0x53000040u,
    0x54000000u,
    0x55000000u,
    0x56000000u,
    0x57000000u,
    0x58000000u,
};

Armv7aPlatformResetState g_qemuVirtResetState{};

Armv7aStackRange stack_range(char& base, char& top)
{
    return Armv7aStackRange{
        .base = reinterpret_cast<std::uintptr_t>(&base),
        .top = reinterpret_cast<std::uintptr_t>(&top),
    };
}
} // namespace

const Armv7aPlatformAddressSpace& armv7a_platform_address_space()
{
    return kQemuVirtAddressSpace;
}

const Armv7aPlatformMmioLayout& armv7a_platform_mmio_layout()
{
    return kQemuVirtMmioLayout;
}

const Armv7aPlatformProbeLayout& armv7a_platform_probe_layout()
{
    return kQemuVirtProbeLayout;
}

const Armv7aPlatformResetState& armv7a_platform_reset_state()
{
    return g_qemuVirtResetState;
}

Armv7aStackRange armv7a_platform_stack_range_for_mode(std::uint32_t psr)
{
    switch (psr & kPsrModeMask) {
    case kModeUnd:
        return stack_range(__und_stack_base, __und_stack_top);
    case kModeAbt:
        return stack_range(__abt_stack_base, __abt_stack_top);
    case kModeIrq:
        return stack_range(__irq_stack_base, __irq_stack_top);
    case kModeFiq:
        return stack_range(__fiq_stack_base, __fiq_stack_top);
    case kModeSvc:
        return stack_range(__svc_stack_base, __svc_stack_top);
    case kModeSys:
        return stack_range(__sys_stack_base, __sys_stack_top);
    default:
        return Armv7aStackRange{};
    }
}

extern "C" void armv7a_platform_debug_trace(const char* text)
{
    register int op asm("r0") = 0x04;
    register const char* ptr asm("r1") = text;
    asm volatile("bkpt 0xab" : : "r"(op), "r"(ptr) : "memory");
}

extern "C" void armv7a_platform_reset_early()
{
    g_qemuVirtResetState.initial_sctlr = armv7a_read_sctlr();
    g_qemuVirtResetState.initial_vbar = armv7a_read_vbar();
    g_qemuVirtResetState.forced_low_vectors =
        armv7a_high_vectors_enabled(g_qemuVirtResetState.initial_sctlr);
    armv7a_ensure_low_vectors();
}

extern "C" void armv7a_platform_install_exception_vectors(const void* vector_base)
{
    armv7a_write_vbar(static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(vector_base)));
    armv7a_data_sync_barrier();
    armv7a_instruction_sync_barrier();
}

extern "C" [[noreturn]] void armv7a_platform_idle_forever()
{
    for (;;) {
        asm volatile("wfe");
    }
}

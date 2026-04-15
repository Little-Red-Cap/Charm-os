#include "armv7a_cpu.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

namespace {
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

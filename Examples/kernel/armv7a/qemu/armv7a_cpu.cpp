#include "armv7a_cpu.hpp"

namespace {
constexpr std::uint32_t kPsrModeMask = 0x1fu;
constexpr std::uint32_t kPsrIrqMask = 1u << 7;
constexpr std::uint32_t kPsrFiqMask = 1u << 6;
constexpr std::uint32_t kIdMmfr0VmsaShift = 0u;
constexpr std::uint32_t kIdMmfr0PmsaShift = 4u;
constexpr std::uint32_t kIdMmfr0FieldMask = 0xfu;
constexpr std::uint32_t kIdPfr1SecurityShift = 4u;
constexpr std::uint32_t kIdPfr1VirtualizationShift = 12u;
constexpr std::uint32_t kIdPfr1GentimerShift = 16u;
constexpr std::uint32_t kIdPfr1FieldMask = 0xfu;
} // namespace

extern "C" void armv7a_data_sync_barrier()
{
    asm volatile("dsb sy" ::: "memory");
}

extern "C" void armv7a_instruction_sync_barrier()
{
    asm volatile("isb" ::: "memory");
}

extern "C" void armv7a_enable_irq()
{
    asm volatile("cpsie i" ::: "memory");
}

extern "C" void armv7a_disable_irq()
{
    asm volatile("cpsid i" ::: "memory");
}

extern "C" void armv7a_enable_fiq()
{
    asm volatile("cpsie f" ::: "memory");
}

extern "C" void armv7a_disable_fiq()
{
    asm volatile("cpsid f" ::: "memory");
}

extern "C" std::uint32_t armv7a_read_cpsr()
{
    std::uint32_t value = 0;
    asm volatile("mrs %0, cpsr" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_mpidr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_id_mmfr0()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c0, c1, 4" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_id_pfr1()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c0, c1, 1" : "=r"(value));
    return value;
}

extern "C" std::uint32_t armv7a_read_sctlr()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(value));
    return value;
}

extern "C" void armv7a_write_sctlr(std::uint32_t value)
{
    asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(value) : "memory");
    armv7a_instruction_sync_barrier();
}

extern "C" std::uint32_t armv7a_read_vbar()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(value));
    return value;
}

extern "C" void armv7a_write_vbar(std::uint32_t value)
{
    asm volatile("mcr p15, 0, %0, c12, c0, 0" : : "r"(value) : "memory");
}

extern "C" void armv7a_svc_smoke_test()
{
    asm volatile("svc #0x43" ::: "memory");
}

std::uint32_t armv7a_id_mmfr0_vmsa_field(std::uint32_t value)
{
    return (value >> kIdMmfr0VmsaShift) & kIdMmfr0FieldMask;
}

std::uint32_t armv7a_id_mmfr0_pmsa_field(std::uint32_t value)
{
    return (value >> kIdMmfr0PmsaShift) & kIdMmfr0FieldMask;
}

std::uint32_t armv7a_id_pfr1_security_field(std::uint32_t value)
{
    return (value >> kIdPfr1SecurityShift) & kIdPfr1FieldMask;
}

std::uint32_t armv7a_id_pfr1_virtualization_field(std::uint32_t value)
{
    return (value >> kIdPfr1VirtualizationShift) & kIdPfr1FieldMask;
}

std::uint32_t armv7a_id_pfr1_gentimer_field(std::uint32_t value)
{
    return (value >> kIdPfr1GentimerShift) & kIdPfr1FieldMask;
}

const char* armv7a_feature_presence_name(std::uint32_t field)
{
    return field == 0u ? "absent" : "present";
}

bool armv7a_irq_masked(std::uint32_t psr)
{
    return (psr & kPsrIrqMask) != 0u;
}

bool armv7a_fiq_masked(std::uint32_t psr)
{
    return (psr & kPsrFiqMask) != 0u;
}

const char* armv7a_mode_name(std::uint32_t psr)
{
    switch (psr & kPsrModeMask) {
    case 0x10u:
        return "usr";
    case 0x11u:
        return "fiq";
    case 0x12u:
        return "irq";
    case 0x13u:
        return "svc";
    case 0x16u:
        return "mon";
    case 0x17u:
        return "abt";
    case 0x1au:
        return "hyp";
    case 0x1bu:
        return "und";
    case 0x1fu:
        return "sys";
    default:
        return "unknown";
    }
}

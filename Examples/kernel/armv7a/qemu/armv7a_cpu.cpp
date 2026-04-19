#include "armv7a_cpu.hpp"

#include "armv7a_runtime_bridge_contract.hpp"

namespace {
constexpr std::uint32_t kSctlrAlignmentCheckMask = 1u << 1;
constexpr std::uint32_t kIdMmfr0VmsaShift = 0u;
constexpr std::uint32_t kIdMmfr0PmsaShift = 4u;
constexpr std::uint32_t kIdMmfr0FieldMask = 0xfu;
constexpr std::uint32_t kIdPfr1SecurityShift = 4u;
constexpr std::uint32_t kIdPfr1VirtualizationShift = 12u;
constexpr std::uint32_t kIdPfr1GentimerShift = 16u;
constexpr std::uint32_t kIdPfr1FieldMask = 0xfu;
constexpr std::uint32_t kArmv7aSvcYieldArg0 = 0x00000001u;
constexpr std::uint32_t kArmv7aSvcYieldArg1 = 0x00000001u;
constexpr std::uint32_t kArmv7aSvcYieldArg2 = 0x00000000u;
constexpr std::uint32_t kArmv7aSvcYieldArg3 = 0x00000000u;
constexpr std::uint32_t kArmv7aSvcSleepArg0 = 0x00000005u;
constexpr std::uint32_t kArmv7aSvcSleepArg1 = 0x00000000u;
constexpr std::uint32_t kArmv7aSvcSleepArg2 = 0x00000001u;
constexpr std::uint32_t kArmv7aSvcSleepArg3 = 0x00000005u;
constexpr std::uint32_t kArmv7aSvcDebugWriteArg0 = 0x00000044u;
constexpr std::uint32_t kArmv7aSvcDebugWriteArg1 = 0x00000000u;
constexpr std::uint32_t kArmv7aSvcDebugWriteArg2 = 0x00000000u;
constexpr std::uint32_t kArmv7aSvcDebugWriteArg3 = 0x00000000u;
constexpr std::uint32_t kArmv7aSvcCapabilityArg0 = 0x00000007u;
constexpr std::uint32_t kArmv7aSvcCapabilityArg1 = 0x00000002u;
constexpr std::uint32_t kArmv7aSvcCapabilityArg2 = 0x00000021u;
constexpr std::uint32_t kArmv7aSvcCapabilityArg3 = 0x00000000u;
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

extern "C" void armv7a_compiler_barrier()
{
    asm volatile("" ::: "memory");
}

extern "C" std::uint32_t armv7a_read_cpsr()
{
    std::uint32_t value = 0;
    asm volatile("mrs %0, cpsr" : "=r"(value));
    return value;
}

extern "C" std::uintptr_t armv7a_read_sp()
{
    std::uintptr_t value = 0;
    asm volatile("mov %0, sp" : "=r"(value));
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

extern "C" void armv7a_branch_to_address(std::uintptr_t target)
{
    asm volatile("bx %0" : : "r"(target) : "memory");
}

extern "C" std::uint32_t armv7a_load_word_relaxed(std::uintptr_t address)
{
    std::uint32_t value = 0;
    asm volatile("ldr %0, [%1]" : "=r"(value) : "r"(address) : "memory");
    return value;
}

extern "C" void armv7a_undefined_instruction()
{
    asm volatile("udf #0" ::: "memory");
}

extern "C" void armv7a_svc_smoke_test()
{
    (void)armv7a_svc_smoke_test_result();
}

extern "C" std::uint32_t armv7a_svc_smoke_test_result()
{
    return armv7a_runtime_yield_call(
        kArmv7aSvcYieldArg0, kArmv7aSvcYieldArg1);
}

extern "C" void armv7a_svc_sleep_smoke_test()
{
    (void)armv7a_svc_sleep_smoke_test_result();
}

extern "C" std::uint32_t armv7a_svc_sleep_smoke_test_result()
{
    return armv7a_runtime_sleep_until_call(
        (static_cast<std::uint64_t>(kArmv7aSvcSleepArg1) << 32u) |
            static_cast<std::uint64_t>(kArmv7aSvcSleepArg0),
        kArmv7aSvcSleepArg2,
        kArmv7aSvcSleepArg3);
}

extern "C" std::uint32_t armv7a_svc_debug_write_smoke_test_result()
{
    register std::uint32_t r0 asm("r0") = kArmv7aSvcDebugWriteArg0;
    register std::uint32_t r1 asm("r1") = kArmv7aSvcDebugWriteArg1;
    register std::uint32_t r2 asm("r2") = kArmv7aSvcDebugWriteArg2;
    register std::uint32_t r3 asm("r3") = kArmv7aSvcDebugWriteArg3;
    asm volatile("svc #0x45" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory");
    return r0;
}

extern "C" std::uint32_t armv7a_svc_capability_call_smoke_test_result()
{
    register std::uint32_t r0 asm("r0") = kArmv7aSvcCapabilityArg0;
    register std::uint32_t r1 asm("r1") = kArmv7aSvcCapabilityArg1;
    register std::uint32_t r2 asm("r2") = kArmv7aSvcCapabilityArg2;
    register std::uint32_t r3 asm("r3") = kArmv7aSvcCapabilityArg3;
    asm volatile("svc #0x46" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory");
    return r0;
}

extern "C" std::uint32_t armv7a_runtime_yield_call(std::uint32_t event_id,
                                                   std::uint32_t event_payload)
{
    register std::uint32_t r0 asm("r0") = event_id;
    register std::uint32_t r1 asm("r1") = event_payload;
    register std::uint32_t r2 asm("r2") = 0u;
    register std::uint32_t r3 asm("r3") = 0u;
    asm volatile("svc #0x43" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory");
    return r0;
}

extern "C" std::uint32_t armv7a_runtime_sleep_until_call(
    std::uint64_t due,
    std::uint32_t event_id,
    std::uint32_t event_payload)
{
    register std::uint32_t r0 asm("r0") =
        static_cast<std::uint32_t>(due & 0xFFFF'FFFFull);
    register std::uint32_t r1 asm("r1") =
        static_cast<std::uint32_t>((due >> 32u) & 0xFFFF'FFFFull);
    register std::uint32_t r2 asm("r2") = event_id;
    register std::uint32_t r3 asm("r3") = event_payload;
    asm volatile("svc #0x44" : "+r"(r0) : "r"(r1), "r"(r2), "r"(r3) : "memory");
    return r0;
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

bool armv7a_alignment_check_enabled(std::uint32_t sctlr)
{
    return (sctlr & kSctlrAlignmentCheckMask) != 0u;
}

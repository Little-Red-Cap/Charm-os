#pragma once

#include <cstdint>

namespace rk3506::armv7a {
    struct GenericTimerCount {
        std::uint32_t lo = 0u;
        std::uint32_t hi = 0u;
    };

    inline constexpr std::uint32_t kCpsrModeMask = 0x1fu;
    inline constexpr std::uint32_t kCpsrThumbState = 1u << 5;
    inline constexpr std::uint32_t kCpsrFiqMasked = 1u << 6;
    inline constexpr std::uint32_t kCpsrIrqMasked = 1u << 7;
    inline constexpr std::uint32_t kCpsrAsyncAbortMasked = 1u << 8;
    inline constexpr std::uint32_t kCpsrBigEndian = 1u << 9;

    inline constexpr std::uint32_t kCpuModeUsr = 0x10u;
    inline constexpr std::uint32_t kCpuModeFiq = 0x11u;
    inline constexpr std::uint32_t kCpuModeIrq = 0x12u;
    inline constexpr std::uint32_t kCpuModeSvc = 0x13u;
    inline constexpr std::uint32_t kCpuModeMon = 0x16u;
    inline constexpr std::uint32_t kCpuModeAbt = 0x17u;
    inline constexpr std::uint32_t kCpuModeHyp = 0x1au;
    inline constexpr std::uint32_t kCpuModeUnd = 0x1bu;
    inline constexpr std::uint32_t kCpuModeSys = 0x1fu;

    inline constexpr std::uint32_t kSctlrMmuEnabled = 1u << 0;
    inline constexpr std::uint32_t kSctlrAlignmentCheckEnabled = 1u << 1;
    inline constexpr std::uint32_t kSctlrDcacheEnabled = 1u << 2;
    inline constexpr std::uint32_t kSctlrBranchPredictionEnabled = 1u << 11;
    inline constexpr std::uint32_t kSctlrIcacheEnabled = 1u << 12;
    inline constexpr std::uint32_t kSctlrHighVectors = 1u << 13;
    inline constexpr std::uint32_t kIdPfr1GenericTimerShift = 16u;
    inline constexpr std::uint32_t kIdFieldMask = 0xfu;

    inline constexpr std::uintptr_t kHighVectorBase = 0xffff0000u;

    inline void compiler_barrier() noexcept
    {
        asm volatile("" ::: "memory");
    }

    inline std::uint32_t read_cpsr() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrs %0, cpsr" : "=r"(value));
        return value;
    }

    inline std::uint32_t read_sctlr() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c1, c0, 0" : "=r"(value));
        return value;
    }

    inline void write_sctlr(std::uint32_t value) noexcept
    {
        asm volatile("mcr p15, 0, %0, c1, c0, 0" : : "r"(value) : "memory");
    }

    inline std::uint32_t read_ctr() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(value));
        return value;
    }

    inline std::uint32_t read_mpidr() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c0, c0, 5" : "=r"(value));
        return value;
    }

    inline std::uint32_t read_id_pfr1() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c0, c1, 1" : "=r"(value));
        return value;
    }

    inline std::uint32_t read_cntfrq() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(value));
        return value;
    }

    inline GenericTimerCount read_cntpct() noexcept
    {
        GenericTimerCount value{};
        asm volatile("isb" ::: "memory");
        asm volatile(
            "mrrc p15, 0, %0, %1, c14" : "=r"(value.lo), "=r"(value.hi));
        return value;
    }

    inline std::uint32_t read_cntp_ctl() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c14, c2, 1" : "=r"(value));
        return value;
    }

    inline void write_cntp_ctl(std::uint32_t value) noexcept
    {
        asm volatile("mcr p15, 0, %0, c14, c2, 1" : : "r"(value) : "memory");
        asm volatile("isb" ::: "memory");
    }

    inline void write_cntp_tval(std::uint32_t value) noexcept
    {
        asm volatile("mcr p15, 0, %0, c14, c2, 0" : : "r"(value) : "memory");
        asm volatile("isb" ::: "memory");
    }

    inline std::uint32_t read_ttbr0() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(value));
        return value;
    }

    inline std::uint32_t read_ttbcr() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c2, c0, 2" : "=r"(value));
        return value;
    }

    inline std::uint32_t read_dacr() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c3, c0, 0" : "=r"(value));
        return value;
    }

    inline std::uintptr_t read_vbar() noexcept
    {
        std::uint32_t value = 0u;
        asm volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(value));
        return static_cast<std::uintptr_t>(value);
    }

    inline void write_vbar(std::uintptr_t value) noexcept
    {
        const auto narrow_value = static_cast<std::uint32_t>(value);
        asm volatile("mcr p15, 0, %0, c12, c0, 0" : : "r"(narrow_value) : "memory");
    }

    inline void data_sync_barrier() noexcept
    {
        asm volatile("dsb sy" ::: "memory");
    }

    inline void instruction_sync_barrier() noexcept
    {
        asm volatile("isb" ::: "memory");
    }

    inline void enable_irq() noexcept
    {
        asm volatile("cpsie i" ::: "memory");
    }

    inline void disable_irq() noexcept
    {
        asm volatile("cpsid i" ::: "memory");
    }

    inline constexpr std::uint32_t decode_ctr_line_size(std::uint32_t encoded_words) noexcept
    {
        return 4u << encoded_words;
    }

    inline constexpr std::uint32_t cpu_mode(std::uint32_t cpsr) noexcept
    {
        return cpsr & kCpsrModeMask;
    }

    inline constexpr bool irq_masked(std::uint32_t cpsr) noexcept
    {
        return (cpsr & kCpsrIrqMasked) != 0u;
    }

    inline constexpr bool fiq_masked(std::uint32_t cpsr) noexcept
    {
        return (cpsr & kCpsrFiqMasked) != 0u;
    }

    inline constexpr bool async_abort_masked(std::uint32_t cpsr) noexcept
    {
        return (cpsr & kCpsrAsyncAbortMasked) != 0u;
    }

    inline constexpr bool thumb_state(std::uint32_t cpsr) noexcept
    {
        return (cpsr & kCpsrThumbState) != 0u;
    }

    inline constexpr bool big_endian(std::uint32_t cpsr) noexcept
    {
        return (cpsr & kCpsrBigEndian) != 0u;
    }

    inline constexpr bool mmu_enabled(std::uint32_t sctlr) noexcept
    {
        return (sctlr & kSctlrMmuEnabled) != 0u;
    }

    inline constexpr bool alignment_check_enabled(std::uint32_t sctlr) noexcept
    {
        return (sctlr & kSctlrAlignmentCheckEnabled) != 0u;
    }

    inline constexpr bool dcache_enabled(std::uint32_t sctlr) noexcept
    {
        return (sctlr & kSctlrDcacheEnabled) != 0u;
    }

    inline constexpr bool branch_prediction_enabled(std::uint32_t sctlr) noexcept
    {
        return (sctlr & kSctlrBranchPredictionEnabled) != 0u;
    }

    inline constexpr bool icache_enabled(std::uint32_t sctlr) noexcept
    {
        return (sctlr & kSctlrIcacheEnabled) != 0u;
    }

    inline constexpr bool high_vectors(std::uint32_t sctlr) noexcept
    {
        return (sctlr & kSctlrHighVectors) != 0u;
    }

    inline constexpr std::uintptr_t vector_base(std::uint32_t sctlr,
                                                std::uintptr_t vbar) noexcept
    {
        return high_vectors(sctlr) ? kHighVectorBase : vbar;
    }

    inline constexpr std::uint32_t generic_timer_field(
        std::uint32_t id_pfr1) noexcept
    {
        return (id_pfr1 >> kIdPfr1GenericTimerShift) & kIdFieldMask;
    }

    inline constexpr bool generic_timer_present(std::uint32_t id_pfr1) noexcept
    {
        return generic_timer_field(id_pfr1) != 0u;
    }

    inline constexpr bool count_advanced(GenericTimerCount first,
                                         GenericTimerCount second) noexcept
    {
        return second.hi > first.hi ||
            (second.hi == first.hi && second.lo > first.lo);
    }

    inline constexpr GenericTimerCount add_ticks(GenericTimerCount value,
                                                 std::uint32_t ticks) noexcept
    {
        const auto next_lo = static_cast<std::uint32_t>(value.lo + ticks);
        return GenericTimerCount{
            next_lo,
            static_cast<std::uint32_t>(value.hi + (next_lo < value.lo ? 1u : 0u)),
        };
    }

    inline constexpr bool count_at_or_after(GenericTimerCount current,
                                            GenericTimerCount limit) noexcept
    {
        return current.hi > limit.hi ||
            (current.hi == limit.hi && current.lo >= limit.lo);
    }

    inline const char* mode_name(std::uint32_t mode) noexcept
    {
        switch (mode) {
        case kCpuModeUsr:
            return "usr";
        case kCpuModeFiq:
            return "fiq";
        case kCpuModeIrq:
            return "irq";
        case kCpuModeSvc:
            return "svc";
        case kCpuModeMon:
            return "mon";
        case kCpuModeAbt:
            return "abt";
        case kCpuModeHyp:
            return "hyp";
        case kCpuModeUnd:
            return "und";
        case kCpuModeSys:
            return "sys";
        default:
            return "unknown";
        }
    }
}

#include "armv7a_arch_timer.hpp"

#include "armv7a_cpu.hpp"

std::uint32_t armv7a_timer_read_cntfrq()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c14, c0, 0" : "=r"(value));
    return value;
}

std::uint64_t armv7a_timer_read_cntpct()
{
    std::uint32_t lo = 0;
    std::uint32_t hi = 0;
    armv7a_instruction_sync_barrier();
    asm volatile("mrrc p15, 0, %0, %1, c14" : "=r"(lo), "=r"(hi));
    return (static_cast<std::uint64_t>(hi) << 32) | lo;
}

std::uint32_t armv7a_timer_read_ctrl()
{
    std::uint32_t value = 0;
    asm volatile("mrc p15, 0, %0, c14, c2, 1" : "=r"(value));
    return value;
}

void armv7a_timer_write_ctrl(std::uint32_t value)
{
    asm volatile("mcr p15, 0, %0, c14, c2, 1" : : "r"(value));
    armv7a_instruction_sync_barrier();
}

void armv7a_timer_write_tval(std::uint32_t value)
{
    asm volatile("mcr p15, 0, %0, c14, c2, 0" : : "r"(value));
    armv7a_instruction_sync_barrier();
}

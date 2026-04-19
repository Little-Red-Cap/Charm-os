#pragma once

#include <cstdint>

struct Armv7aRuntimeLiveObservation {
    bool task_ready = false;
    bool trap_ready = false;
    bool timer_ready = false;
    bool tick_ready = false;
    bool idle_ready = false;
    bool worker_ready = false;
    bool irq_enabled_after_yield = false;
    bool irq_enabled_after_sleep = false;
    bool timer_arm_seen = false;
    bool timer_armed = false;
    bool timer_irq_seen = false;
    bool timer_line_enabled = false;
    bool timer_line_pending = false;
    bool timer_line_active = false;
    bool wait_timeout_seen = false;
    bool yield_trap_ok = false;
    bool sleep_trap_ok = false;
    bool runtime_trace_worker_bootstrap = false;
    bool runtime_trace_yield = false;
    bool runtime_trace_sleep = false;
    bool runtime_trace_tick = false;
    bool runtime_trace_idle = false;
    bool trap_trace_yield = false;
    bool trap_trace_sleep = false;
    bool yield_frame_sampled = false;
    bool sleep_frame_sampled = false;
    std::uint32_t worker_resumes = 0u;
    std::uint32_t idle_runs = 0u;
    std::uint32_t cpsr_after_enable_irq = 0u;
    std::uint32_t cpsr_before_bootstrap = 0u;
    std::uint32_t cpsr_after_bootstrap = 0u;
    std::uint32_t cpsr_before_yield = 0u;
    std::uint32_t cpsr_after_yield = 0u;
    std::uint32_t cpsr_before_sleep = 0u;
    std::uint32_t cpsr_after_sleep = 0u;
    std::uint32_t cpsr_wait_timeout = 0u;
    std::uint32_t timer_arm_ticks = 0u;
    std::uint32_t timer_ctrl = 0u;
    std::uint32_t gicd_ctlr = 0u;
    std::uint32_t gicc_ctlr = 0u;
    std::uint32_t hppir = 0u;
    std::uint32_t yield_origin_psr = 0u;
    std::uint32_t yield_handler_psr = 0u;
    std::uint32_t sleep_origin_psr = 0u;
    std::uint32_t sleep_handler_psr = 0u;
    std::uint64_t wake_due = 0u;
    std::uint64_t last_tick_now = 0u;
    std::uint64_t last_trap_value = 0u;
};

constexpr bool armv7a_runtime_live_ready(
    const Armv7aRuntimeLiveObservation& observation) noexcept
{
    return observation.task_ready && observation.trap_ready &&
           observation.timer_ready && observation.tick_ready &&
           observation.idle_ready && observation.worker_ready;
}

Armv7aRuntimeLiveObservation armv7a_run_runtime_live_observation() noexcept;
void armv7a_print_runtime_live_observation();

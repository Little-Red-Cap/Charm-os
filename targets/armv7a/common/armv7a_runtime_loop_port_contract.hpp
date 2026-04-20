#pragma once

#include <cstdint>

enum class Armv7aRuntimeLoopEventPayloadKind : std::uint8_t {
    none = 0,
    u32,
    u64,
};

struct Armv7aRuntimeLoopEvent {
    std::uint32_t id = 0u;
    std::uint64_t payload = 0u;
    Armv7aRuntimeLoopEventPayloadKind payload_kind =
        Armv7aRuntimeLoopEventPayloadKind::none;
};

constexpr Armv7aRuntimeLoopEvent armv7a_make_runtime_loop_event(
    std::uint32_t id) noexcept
{
    return Armv7aRuntimeLoopEvent{
        .id = id,
    };
}

constexpr Armv7aRuntimeLoopEvent armv7a_make_runtime_loop_event_u32(
    std::uint32_t id,
    std::uint32_t payload) noexcept
{
    return Armv7aRuntimeLoopEvent{
        .id = id,
        .payload = payload,
        .payload_kind = Armv7aRuntimeLoopEventPayloadKind::u32,
    };
}

constexpr Armv7aRuntimeLoopEvent armv7a_make_runtime_loop_event_u64(
    std::uint32_t id,
    std::uint64_t payload) noexcept
{
    return Armv7aRuntimeLoopEvent{
        .id = id,
        .payload = payload,
        .payload_kind = Armv7aRuntimeLoopEventPayloadKind::u64,
    };
}

struct Armv7aRuntimeLoopPortContract {
    void* ctx = nullptr;
    std::uint64_t (*advance_tick)(void* ctx, std::uint64_t now) noexcept =
        nullptr;
    bool (*defer_from_isr)(void* ctx,
                           std::uint64_t task,
                           Armv7aRuntimeLoopEvent event) noexcept = nullptr;
    bool (*bootstrap_idle_default)(void* ctx) noexcept = nullptr;
    bool (*bootstrap_idle_event)(void* ctx,
                                 Armv7aRuntimeLoopEvent event) noexcept =
        nullptr;
    bool (*bootstrap_worker)(void* ctx,
                             std::uint64_t task,
                             Armv7aRuntimeLoopEvent event) noexcept = nullptr;
    std::uint64_t (*run_once_or_idle)(void* ctx, std::uint64_t now) noexcept =
        nullptr;
};

constexpr bool armv7a_runtime_loop_port_ready(
    const Armv7aRuntimeLoopPortContract& port) noexcept
{
    return port.advance_tick != nullptr && port.defer_from_isr != nullptr &&
           port.bootstrap_idle_default != nullptr &&
           port.bootstrap_idle_event != nullptr &&
           port.bootstrap_worker != nullptr &&
           port.run_once_or_idle != nullptr;
}

inline std::uint64_t armv7a_runtime_loop_port_advance_tick(
    const Armv7aRuntimeLoopPortContract& port,
    std::uint64_t now) noexcept
{
    return port.advance_tick != nullptr ? port.advance_tick(port.ctx, now) : 0u;
}

inline bool armv7a_runtime_loop_port_defer_from_isr(
    const Armv7aRuntimeLoopPortContract& port,
    std::uint64_t task,
    Armv7aRuntimeLoopEvent event) noexcept
{
    return port.defer_from_isr != nullptr &&
           port.defer_from_isr(port.ctx, task, event);
}

inline bool armv7a_runtime_loop_port_bootstrap_idle(
    const Armv7aRuntimeLoopPortContract& port) noexcept
{
    return port.bootstrap_idle_default != nullptr &&
           port.bootstrap_idle_default(port.ctx);
}

inline bool armv7a_runtime_loop_port_bootstrap_idle(
    const Armv7aRuntimeLoopPortContract& port,
    Armv7aRuntimeLoopEvent event) noexcept
{
    return port.bootstrap_idle_event != nullptr &&
           port.bootstrap_idle_event(port.ctx, event);
}

inline bool armv7a_runtime_loop_port_bootstrap_worker(
    const Armv7aRuntimeLoopPortContract& port,
    std::uint64_t task,
    Armv7aRuntimeLoopEvent event) noexcept
{
    return port.bootstrap_worker != nullptr &&
           port.bootstrap_worker(port.ctx, task, event);
}

inline std::uint64_t armv7a_runtime_loop_port_run_once_or_idle(
    const Armv7aRuntimeLoopPortContract& port,
    std::uint64_t now) noexcept
{
    return port.run_once_or_idle != nullptr
        ? port.run_once_or_idle(port.ctx, now)
        : 0u;
}

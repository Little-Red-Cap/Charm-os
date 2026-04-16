#pragma once

#include <cstdint>

#include "armv7a_interrupt_contract.hpp"

// This contract captures the minimum ARMv7-A ingress seams that a leaf target
// must eventually provide before the generic kernel scheduler/thread/timer
// core can run on bare metal without knowing board-private details.
enum class Armv7aKernelTickMode : std::uint8_t {
    none = 0,
    one_shot,
    periodic,
};

enum class Armv7aKernelContextSwitchModel : std::uint8_t {
    none = 0,
    software_frame,
    exception_return,
};

struct Armv7aKernelExceptionPort {
    void* ctx = nullptr;
    std::uintptr_t preferred_vector_base = 0u;
    bool (*install_vectors)(void* ctx, std::uintptr_t vector_base) noexcept = nullptr;
    bool (*vectors_active)(void* ctx, std::uintptr_t vector_base) noexcept = nullptr;
};

struct Armv7aKernelInterruptPort {
    void* ctx = nullptr;
    bool (*mask_local_irq)(void* ctx) noexcept = nullptr;
    bool (*unmask_local_irq)(void* ctx) noexcept = nullptr;
    bool (*enable_scheduler_route)(void* ctx) noexcept = nullptr;
    bool (*disable_scheduler_route)(void* ctx) noexcept = nullptr;
    Armv7aPlatformInterruptAcknowledge (*acknowledge)(void* ctx) noexcept = nullptr;
    bool (*complete)(void* ctx, std::uint32_t raw_acknowledge) noexcept = nullptr;
};

struct Armv7aKernelTimerPort {
    void* ctx = nullptr;
    Armv7aKernelTickMode tick_mode = Armv7aKernelTickMode::none;
    Armv7aPlatformInterruptRoute tick_route = Armv7aPlatformInterruptRoute::kIrq;
    std::uint32_t frequency_hz = 0u;
    bool (*arm_tick)(void* ctx, std::uint32_t ticks) noexcept = nullptr;
    bool (*stop_tick)(void* ctx) noexcept = nullptr;
};

struct Armv7aKernelContextPort {
    void* ctx = nullptr;
    Armv7aKernelContextSwitchModel switch_model =
        Armv7aKernelContextSwitchModel::none;
    std::uintptr_t (*prepare_initial_frame)(void* ctx,
                                            std::uintptr_t stack_top,
                                            std::uintptr_t entry_addr,
                                            std::uintptr_t argument) noexcept = nullptr;
    bool (*switch_context)(void* ctx,
                           std::uintptr_t* outgoing_sp,
                           std::uintptr_t incoming_sp) noexcept = nullptr;
};

struct Armv7aKernelPortContract {
    Armv7aKernelExceptionPort exception{};
    Armv7aKernelInterruptPort interrupt{};
    Armv7aKernelTimerPort timer{};
    Armv7aKernelContextPort context{};
};

constexpr bool armv7a_kernel_exception_port_ready(
    const Armv7aKernelExceptionPort& port) noexcept
{
    return port.preferred_vector_base != 0u && port.install_vectors != nullptr &&
           port.vectors_active != nullptr;
}

constexpr bool armv7a_kernel_interrupt_port_ready(
    const Armv7aKernelInterruptPort& port) noexcept
{
    return port.mask_local_irq != nullptr && port.unmask_local_irq != nullptr &&
           port.enable_scheduler_route != nullptr &&
           port.disable_scheduler_route != nullptr &&
           port.acknowledge != nullptr && port.complete != nullptr;
}

constexpr bool armv7a_kernel_timer_port_ready(
    const Armv7aKernelTimerPort& port) noexcept
{
    return port.tick_mode != Armv7aKernelTickMode::none &&
           port.frequency_hz != 0u && port.arm_tick != nullptr &&
           port.stop_tick != nullptr;
}

constexpr bool armv7a_kernel_context_port_ready(
    const Armv7aKernelContextPort& port) noexcept
{
    return port.switch_model != Armv7aKernelContextSwitchModel::none &&
           port.prepare_initial_frame != nullptr &&
           port.switch_context != nullptr;
}

constexpr bool armv7a_kernel_tick_runtime_ready(
    const Armv7aKernelPortContract& contract) noexcept
{
    return armv7a_kernel_exception_port_ready(contract.exception) &&
           armv7a_kernel_interrupt_port_ready(contract.interrupt) &&
           armv7a_kernel_timer_port_ready(contract.timer);
}

constexpr bool armv7a_kernel_thread_runtime_ready(
    const Armv7aKernelPortContract& contract) noexcept
{
    return armv7a_kernel_tick_runtime_ready(contract) &&
           armv7a_kernel_context_port_ready(contract.context);
}

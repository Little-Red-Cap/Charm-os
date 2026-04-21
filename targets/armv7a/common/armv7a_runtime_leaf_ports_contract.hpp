#pragma once

#include "armv7a_interrupt_runtime_hook_contract.hpp"
#include "armv7a_kernel_port_contract.hpp"
#include "armv7a_runtime_loop_port_contract.hpp"
#include "armv7a_runtime_thread_port_contract.hpp"
#include "armv7a_runtime_trap_call_port_contract.hpp"
#include "armv7a_runtime_trap_dispatch_contract.hpp"

struct Armv7aRuntimeLeafPortsContract {
    Armv7aKernelPortContract kernel{};
    Armv7aInterruptRuntimeHook interrupt_hook{};
    Armv7aRuntimeTrapDispatchPort trap_dispatch{};
    Armv7aRuntimeTrapCallPortContract trap_call{};
    Armv7aRuntimeThreadPortContract runtime_thread{};
    Armv7aRuntimeLoopPortContract runtime_loop{};
};

constexpr bool armv7a_runtime_leaf_ports_equal(
    const Armv7aRuntimeLeafPortsContract& lhs,
    const Armv7aRuntimeLeafPortsContract& rhs) noexcept
{
    return lhs.kernel.exception.ctx == rhs.kernel.exception.ctx &&
           lhs.kernel.exception.preferred_vector_base ==
               rhs.kernel.exception.preferred_vector_base &&
           lhs.kernel.exception.install_vectors ==
               rhs.kernel.exception.install_vectors &&
           lhs.kernel.exception.vectors_active ==
               rhs.kernel.exception.vectors_active &&
           lhs.kernel.interrupt.ctx == rhs.kernel.interrupt.ctx &&
           lhs.kernel.interrupt.mask_local_irq ==
               rhs.kernel.interrupt.mask_local_irq &&
           lhs.kernel.interrupt.unmask_local_irq ==
               rhs.kernel.interrupt.unmask_local_irq &&
           lhs.kernel.interrupt.enable_scheduler_route ==
               rhs.kernel.interrupt.enable_scheduler_route &&
           lhs.kernel.interrupt.disable_scheduler_route ==
               rhs.kernel.interrupt.disable_scheduler_route &&
           lhs.kernel.interrupt.acknowledge ==
               rhs.kernel.interrupt.acknowledge &&
           lhs.kernel.interrupt.complete == rhs.kernel.interrupt.complete &&
           lhs.kernel.timer.ctx == rhs.kernel.timer.ctx &&
           lhs.kernel.timer.tick_mode == rhs.kernel.timer.tick_mode &&
           lhs.kernel.timer.tick_route == rhs.kernel.timer.tick_route &&
           lhs.kernel.timer.frequency_hz == rhs.kernel.timer.frequency_hz &&
           lhs.kernel.timer.arm_tick == rhs.kernel.timer.arm_tick &&
           lhs.kernel.timer.stop_tick == rhs.kernel.timer.stop_tick &&
           lhs.kernel.context.ctx == rhs.kernel.context.ctx &&
           lhs.kernel.context.switch_model == rhs.kernel.context.switch_model &&
           lhs.kernel.context.prepare_initial_frame ==
               rhs.kernel.context.prepare_initial_frame &&
           lhs.kernel.context.switch_context ==
               rhs.kernel.context.switch_context &&
           lhs.kernel.current.ctx == rhs.kernel.current.ctx &&
           lhs.kernel.current.capture == rhs.kernel.current.capture &&
           lhs.interrupt_hook.ctx == rhs.interrupt_hook.ctx &&
           lhs.interrupt_hook.on_delivery == rhs.interrupt_hook.on_delivery &&
           lhs.trap_dispatch.ctx == rhs.trap_dispatch.ctx &&
           lhs.trap_dispatch.dispatch_frame == rhs.trap_dispatch.dispatch_frame &&
           lhs.trap_call.ctx == rhs.trap_call.ctx &&
           lhs.trap_call.yield_current == rhs.trap_call.yield_current &&
           lhs.trap_call.sleep_current_until ==
               rhs.trap_call.sleep_current_until &&
           lhs.runtime_thread.ctx == rhs.runtime_thread.ctx &&
           lhs.runtime_thread.yield_current ==
               rhs.runtime_thread.yield_current &&
           lhs.runtime_thread.sleep_current_until ==
               rhs.runtime_thread.sleep_current_until &&
           lhs.runtime_loop.ctx == rhs.runtime_loop.ctx &&
           lhs.runtime_loop.advance_tick == rhs.runtime_loop.advance_tick &&
           lhs.runtime_loop.defer_from_isr ==
               rhs.runtime_loop.defer_from_isr &&
           lhs.runtime_loop.bootstrap_idle_default ==
               rhs.runtime_loop.bootstrap_idle_default &&
           lhs.runtime_loop.bootstrap_idle_event ==
               rhs.runtime_loop.bootstrap_idle_event &&
           lhs.runtime_loop.bootstrap_worker ==
               rhs.runtime_loop.bootstrap_worker &&
           lhs.runtime_loop.run_once_or_idle ==
               rhs.runtime_loop.run_once_or_idle;
}

constexpr bool armv7a_runtime_leaf_ports_exception_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_exception_port_ready(contract.kernel.exception);
}

constexpr bool armv7a_runtime_leaf_ports_interrupt_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_interrupt_port_ready(contract.kernel.interrupt);
}

constexpr bool armv7a_runtime_leaf_ports_timer_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_timer_port_ready(contract.kernel.timer);
}

constexpr bool armv7a_runtime_leaf_ports_context_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_context_port_ready(contract.kernel.context);
}

constexpr bool armv7a_runtime_leaf_ports_current_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_kernel_current_port_ready(contract.kernel.current);
}

constexpr bool armv7a_runtime_leaf_ports_hook_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_interrupt_runtime_hook_ready(contract.interrupt_hook);
}

constexpr bool armv7a_runtime_leaf_ports_trap_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_runtime_trap_dispatch_port_ready(contract.trap_dispatch);
}

constexpr bool armv7a_runtime_leaf_ports_call_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_runtime_trap_call_port_ready(contract.trap_call);
}

constexpr bool armv7a_runtime_leaf_ports_thread_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_runtime_thread_port_ready(contract.runtime_thread);
}

constexpr bool armv7a_runtime_leaf_ports_loop_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_runtime_loop_port_ready(contract.runtime_loop);
}

constexpr bool armv7a_runtime_leaf_ports_ready(
    const Armv7aRuntimeLeafPortsContract& contract) noexcept
{
    return armv7a_runtime_leaf_ports_exception_ready(contract) &&
           armv7a_runtime_leaf_ports_interrupt_ready(contract) &&
           armv7a_runtime_leaf_ports_timer_ready(contract) &&
           armv7a_runtime_leaf_ports_context_ready(contract) &&
           armv7a_runtime_leaf_ports_current_ready(contract) &&
           armv7a_runtime_leaf_ports_hook_ready(contract) &&
           armv7a_runtime_leaf_ports_trap_ready(contract) &&
           armv7a_runtime_leaf_ports_call_ready(contract) &&
           armv7a_runtime_leaf_ports_thread_ready(contract) &&
           armv7a_runtime_leaf_ports_loop_ready(contract);
}

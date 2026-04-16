#include "armv7a_context_switch.hpp"
#include "armv7a_kernel_port.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_mmu.hpp"
#include "armv7a_platform.hpp"

#include <cstdint>

extern "C" void armv7a_vector_table();

namespace {
constexpr std::uint32_t kTimerCtrlEnable = 1u << 0;

bool armv7a_kernel_install_vectors(void*, std::uintptr_t vector_base) noexcept
{
    armv7a_ensure_low_vectors();
    armv7a_platform_install_exception_vectors(
        reinterpret_cast<const void*>(vector_base));
    return armv7a_read_vbar() == vector_base &&
           !armv7a_high_vectors_enabled(armv7a_read_sctlr());
}

bool armv7a_kernel_vectors_active(void*, std::uintptr_t vector_base) noexcept
{
    return armv7a_read_vbar() == vector_base &&
           !armv7a_high_vectors_enabled(armv7a_read_sctlr());
}

bool armv7a_kernel_mask_local_irq(void*) noexcept
{
    armv7a_disable_irq();
    return armv7a_irq_masked(armv7a_read_cpsr());
}

bool armv7a_kernel_unmask_local_irq(void*) noexcept
{
    armv7a_enable_irq();
    return !armv7a_irq_masked(armv7a_read_cpsr());
}

bool armv7a_kernel_enable_scheduler_route(void*) noexcept
{
    armv7a_platform_enable_interrupt_controller(
        Armv7aPlatformInterruptRoute::kIrq);
    const auto state = armv7a_platform_interrupt_controller_state();
    return state.distributor_control != 0u && state.cpu_control != 0u;
}

bool armv7a_kernel_disable_scheduler_route(void*) noexcept
{
    armv7a_platform_disable_interrupt_controller();
    const auto state = armv7a_platform_interrupt_controller_state();
    return state.distributor_control == 0u && state.cpu_control == 0u;
}

Armv7aPlatformInterruptAcknowledge armv7a_kernel_acknowledge_interrupt(
    void*) noexcept
{
    return armv7a_platform_acknowledge_interrupt();
}

bool armv7a_kernel_complete_interrupt(void*,
                                      std::uint32_t raw_acknowledge) noexcept
{
    armv7a_platform_complete_interrupt(raw_acknowledge);
    return true;
}

bool armv7a_kernel_arm_tick(void*, std::uint32_t ticks) noexcept
{
    armv7a_platform_timer_start_oneshot(ticks);
    return (armv7a_platform_timer_control() & kTimerCtrlEnable) != 0u;
}

bool armv7a_kernel_stop_tick(void*) noexcept
{
    armv7a_platform_timer_stop();
    return (armv7a_platform_timer_control() & kTimerCtrlEnable) == 0u;
}

std::uintptr_t armv7a_kernel_prepare_initial_frame(void*,
                                                   std::uintptr_t stack_top,
                                                   std::uintptr_t entry_addr,
                                                   std::uintptr_t argument) noexcept
{
    return armv7a_prepare_cooperative_thread_context(
        stack_top, entry_addr, argument);
}

bool armv7a_kernel_switch_context(void*,
                                  std::uintptr_t* outgoing_sp,
                                  std::uintptr_t incoming_sp) noexcept
{
    if (outgoing_sp == nullptr || incoming_sp == 0u) {
        return false;
    }

    return armv7a_context_switch(outgoing_sp, incoming_sp);
}

const char* armv7a_kernel_tick_mode_name(Armv7aKernelTickMode mode) noexcept
{
    switch (mode) {
    case Armv7aKernelTickMode::one_shot:
        return "oneshot";
    case Armv7aKernelTickMode::periodic:
        return "periodic";
    case Armv7aKernelTickMode::none:
    default:
        return "none";
    }
}

const char* armv7a_kernel_context_switch_model_name(
    Armv7aKernelContextSwitchModel model) noexcept
{
    switch (model) {
    case Armv7aKernelContextSwitchModel::software_frame:
        return "software-frame";
    case Armv7aKernelContextSwitchModel::exception_return:
        return "exception-return";
    case Armv7aKernelContextSwitchModel::none:
    default:
        return "none";
    }
}
} // namespace

Armv7aKernelPortContract armv7a_make_qemu_kernel_port_contract() noexcept
{
    return Armv7aKernelPortContract{
        .exception =
            Armv7aKernelExceptionPort{
                .preferred_vector_base =
                    reinterpret_cast<std::uintptr_t>(&armv7a_vector_table),
                .install_vectors = &armv7a_kernel_install_vectors,
                .vectors_active = &armv7a_kernel_vectors_active,
            },
        .interrupt =
            Armv7aKernelInterruptPort{
                .mask_local_irq = &armv7a_kernel_mask_local_irq,
                .unmask_local_irq = &armv7a_kernel_unmask_local_irq,
                .enable_scheduler_route =
                    &armv7a_kernel_enable_scheduler_route,
                .disable_scheduler_route =
                    &armv7a_kernel_disable_scheduler_route,
                .acknowledge = &armv7a_kernel_acknowledge_interrupt,
                .complete = &armv7a_kernel_complete_interrupt,
            },
        .timer =
            Armv7aKernelTimerPort{
                .tick_mode = Armv7aKernelTickMode::one_shot,
                .tick_route = Armv7aPlatformInterruptRoute::kIrq,
                .frequency_hz = armv7a_platform_timer_frequency_hz(),
                .arm_tick = &armv7a_kernel_arm_tick,
                .stop_tick = &armv7a_kernel_stop_tick,
            },
        .context =
            Armv7aKernelContextPort{
                .switch_model = Armv7aKernelContextSwitchModel::software_frame,
                .prepare_initial_frame = &armv7a_kernel_prepare_initial_frame,
                .switch_context = &armv7a_kernel_switch_context,
            },
    };
}

void armv7a_print_kernel_port_status()
{
    const auto contract = armv7a_make_qemu_kernel_port_contract();
    armv7a_platform_early_console_puts("ARMv7-A kernel ingress, vector-base=0x");
    armv7a_diag_put_hex(contract.exception.preferred_vector_base);
    armv7a_platform_early_console_puts(", tick-mode=");
    armv7a_platform_early_console_puts(
        armv7a_kernel_tick_mode_name(contract.timer.tick_mode));
    armv7a_platform_early_console_puts(", tick-route=");
    armv7a_platform_early_console_puts(
        armv7a_interrupt_route_name(contract.timer.tick_route));
    armv7a_platform_early_console_puts(", timer-hz=");
    armv7a_diag_put_dec(contract.timer.frequency_hz);
    armv7a_platform_early_console_puts(", exception=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_kernel_exception_port_ready(contract.exception)));
    armv7a_platform_early_console_puts(", interrupt=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_kernel_interrupt_port_ready(contract.interrupt)));
    armv7a_platform_early_console_puts(", timer=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_kernel_timer_port_ready(contract.timer)));
    armv7a_platform_early_console_puts(", context-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_kernel_context_port_ready(contract.context)));
    armv7a_platform_early_console_puts(", context-model=");
    armv7a_platform_early_console_puts(
        armv7a_kernel_context_switch_model_name(
            contract.context.switch_model));
    armv7a_platform_early_console_puts(", tick-runtime=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_kernel_tick_runtime_ready(contract)));
    armv7a_platform_early_console_puts(", thread-runtime=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_kernel_thread_runtime_ready(contract)));
    armv7a_platform_early_console_puts("\r\n");
}

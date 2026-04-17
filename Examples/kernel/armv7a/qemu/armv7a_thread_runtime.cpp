#include "armv7a_thread_runtime.hpp"

#include "armv7a_context_smoke.hpp"
#include "armv7a_context_switch.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_kernel_port.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_current.hpp"

#include <array>
#include <cstdint>

namespace {
constexpr std::uint64_t kArmv7aThreadRuntimeTask = 0x0000000059537001ull;
constexpr std::uint64_t kArmv7aThreadRuntimeStack = 0x000000005200B000ull;
constexpr std::uintptr_t kArmv7aThreadRuntimeEntry =
    static_cast<std::uintptr_t>(0x40204000u);
constexpr std::uintptr_t kArmv7aThreadRuntimeArgument =
    static_cast<std::uintptr_t>(0x40205000u);

alignas(8) std::array<std::uint8_t, 1024> g_armv7aThreadRuntimeStack{};

const char* armv7a_thread_runtime_kind_name(
    const Armv7aThreadContextFrameObservation& observation) noexcept
{
    return armv7a_thread_context_frame_uses_cooperative_sys(observation)
        ? "cooperative-sys"
        : "none";
}
} // namespace

Armv7aThreadRuntimeObservation armv7a_capture_thread_runtime_observation() noexcept
{
    const auto contract = armv7a_make_qemu_kernel_port_contract();
    const auto stack_base = reinterpret_cast<std::uintptr_t>(
        g_armv7aThreadRuntimeStack.data());
    const auto stack_top = stack_base + g_armv7aThreadRuntimeStack.size();

    const auto expected_current = Armv7aRuntimeCurrentContext{
        .stack_pointer = kArmv7aThreadRuntimeStack,
        .task = kArmv7aThreadRuntimeTask,
        .task_valid = true,
    };
    armv7a_publish_runtime_current_context(expected_current);

    Armv7aRuntimeCurrentContext captured_current{};
    const auto current_seen = armv7a_runtime_current_context_port_capture(
        contract.current, captured_current);

    const auto prepared_sp = contract.context.prepare_initial_frame(
        contract.context.ctx,
        stack_top,
        kArmv7aThreadRuntimeEntry,
        kArmv7aThreadRuntimeArgument);
    const auto frame = armv7a_make_thread_context_observation(
        stack_base,
        stack_top,
        prepared_sp,
        kArmv7aThreadRuntimeEntry,
        kArmv7aThreadRuntimeArgument);
    const auto switch_smoke = armv7a_context_switch_smoke_last_observation();

    armv7a_clear_runtime_current_context();

    return Armv7aThreadRuntimeObservation{
        .task = captured_current.task,
        .current_stack_pointer = captured_current.stack_pointer,
        .prepared_sp = prepared_sp,
        .current_ready =
            armv7a_kernel_current_port_ready(contract.current) && current_seen &&
            captured_current.task_valid &&
            captured_current.task == expected_current.task &&
            captured_current.stack_pointer == expected_current.stack_pointer,
        .prepare_ready =
            armv7a_kernel_context_port_ready(contract.context) &&
            armv7a_thread_context_frame_ready(frame) &&
            armv7a_thread_context_frame_uses_cooperative_sys(frame),
        .switch_ready =
            switch_smoke.entry_seen && switch_smoke.resumed_seen &&
            !switch_smoke.unexpected_return && switch_smoke.round_trip,
        .runtime_ready = armv7a_kernel_thread_runtime_ready(contract),
    };
}

void armv7a_print_thread_runtime_observation()
{
    const auto observation = armv7a_capture_thread_runtime_observation();
    const auto frame = armv7a_make_thread_context_observation(
        reinterpret_cast<std::uintptr_t>(g_armv7aThreadRuntimeStack.data()),
        reinterpret_cast<std::uintptr_t>(g_armv7aThreadRuntimeStack.data()) +
            g_armv7aThreadRuntimeStack.size(),
        observation.prepared_sp,
        kArmv7aThreadRuntimeEntry,
        kArmv7aThreadRuntimeArgument);

    armv7a_platform_early_console_puts("ARMv7-A thread runtime, kind=");
    armv7a_platform_early_console_puts(armv7a_thread_runtime_kind_name(frame));
    armv7a_platform_early_console_puts(", task=0x");
    armv7a_diag_put_hex64(observation.task, 16);
    armv7a_platform_early_console_puts(", current-sp=0x");
    armv7a_diag_put_hex64(observation.current_stack_pointer, 16);
    armv7a_platform_early_console_puts(", prepared-sp=0x");
    armv7a_diag_put_hex(observation.prepared_sp);
    armv7a_platform_early_console_puts(", current=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.current_ready));
    armv7a_platform_early_console_puts(", prepare=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.prepare_ready));
    armv7a_platform_early_console_puts(", switch=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.switch_ready));
    armv7a_platform_early_console_puts(", runtime=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_thread_runtime_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

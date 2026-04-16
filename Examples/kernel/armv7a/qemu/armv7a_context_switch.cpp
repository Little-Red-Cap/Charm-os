#include "armv7a_context_switch.hpp"

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"

#include <cstddef>
#include <cstdint>

extern "C" void armv7a_context_thread_trampoline();
extern "C" [[noreturn]] void armv7a_context_thread_returned();

namespace {
struct Armv7aCooperativeThreadFrame {
    std::uint32_t r4_entry;
    std::uint32_t r5_argument;
    std::uint32_t r6;
    std::uint32_t r7;
    std::uint32_t r8;
    std::uint32_t r9;
    std::uint32_t r10;
    std::uint32_t r11;
    std::uint32_t ip_return_target;
    std::uint32_t lr_resume_target;
};

static_assert(sizeof(Armv7aCooperativeThreadFrame) ==
              kArmv7aCooperativeThreadFrameSize);

constexpr std::uintptr_t align_down(std::uintptr_t value,
                                    std::uintptr_t alignment) noexcept
{
    return value & ~(alignment - 1u);
}

const char* armv7a_thread_context_kind_name(
    Armv7aThreadContextFrameKind kind) noexcept
{
    switch (kind) {
    case Armv7aThreadContextFrameKind::cooperative_sys:
        return "cooperative-sys";
    case Armv7aThreadContextFrameKind::none:
    default:
        return "none";
    }
}
} // namespace

std::uintptr_t armv7a_prepare_cooperative_thread_context(
    std::uintptr_t stack_top,
    std::uintptr_t entry_addr,
    std::uintptr_t argument) noexcept
{
    if (stack_top == 0u || entry_addr == 0u) {
        return 0u;
    }

    const auto aligned_top = align_down(
        stack_top, kArmv7aCooperativeThreadFrameAlignment);
    if (aligned_top < kArmv7aCooperativeThreadFrameSize) {
        return 0u;
    }

    const auto prepared_sp = aligned_top - kArmv7aCooperativeThreadFrameSize;
    auto* frame =
        reinterpret_cast<Armv7aCooperativeThreadFrame*>(prepared_sp);
    *frame = Armv7aCooperativeThreadFrame{
        .r4_entry = static_cast<std::uint32_t>(entry_addr),
        .r5_argument = static_cast<std::uint32_t>(argument),
        .r6 = 0u,
        .r7 = 0u,
        .r8 = 0u,
        .r9 = 0u,
        .r10 = 0u,
        .r11 = 0u,
        .ip_return_target =
            static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(
                    &armv7a_context_thread_returned)),
        .lr_resume_target =
            static_cast<std::uint32_t>(
                reinterpret_cast<std::uintptr_t>(
                    &armv7a_context_thread_trampoline)),
    };
    return prepared_sp;
}

Armv7aThreadContextFrameObservation armv7a_make_thread_context_observation(
    std::uintptr_t stack_base,
    std::uintptr_t stack_top,
    std::uintptr_t prepared_sp,
    std::uintptr_t entry_addr,
    std::uintptr_t argument) noexcept
{
    return Armv7aThreadContextFrameObservation{
        .kind = Armv7aThreadContextFrameKind::cooperative_sys,
        .stack_base = stack_base,
        .stack_top = stack_top,
        .prepared_sp = prepared_sp,
        .resume_pc =
            reinterpret_cast<std::uintptr_t>(&armv7a_context_thread_trampoline),
        .return_pc =
            reinterpret_cast<std::uintptr_t>(&armv7a_context_thread_returned),
        .entry_pc = entry_addr,
        .argument = argument,
    };
}

void armv7a_print_thread_context_frame(
    const Armv7aThreadContextFrameObservation& observation)
{
    armv7a_platform_early_console_puts(
        "ARMv7-A thread frame, kind=");
    armv7a_platform_early_console_puts(
        armv7a_thread_context_kind_name(observation.kind));
    armv7a_platform_early_console_puts(", stack-base=0x");
    armv7a_diag_put_hex(observation.stack_base);
    armv7a_platform_early_console_puts(", stack-top=0x");
    armv7a_diag_put_hex(observation.stack_top);
    armv7a_platform_early_console_puts(", prepared-sp=0x");
    armv7a_diag_put_hex(observation.prepared_sp);
    armv7a_platform_early_console_puts(", resume=0x");
    armv7a_diag_put_hex(observation.resume_pc);
    armv7a_platform_early_console_puts(", return=0x");
    armv7a_diag_put_hex(observation.return_pc);
    armv7a_platform_early_console_puts(", entry=0x");
    armv7a_diag_put_hex(observation.entry_pc);
    armv7a_platform_early_console_puts(", arg=0x");
    armv7a_diag_put_hex(observation.argument);
    armv7a_platform_early_console_puts(", aligned=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_thread_context_frame_aligned(observation)));
    armv7a_platform_early_console_puts(", in-range=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_thread_context_frame_in_range(observation)));
    armv7a_platform_early_console_puts(", ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_thread_context_frame_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

extern "C" [[noreturn]] void armv7a_context_thread_returned()
{
    armv7a_platform_early_console_puts(
        "ARMv7-A thread return trap, entry returned unexpectedly\r\n");
    armv7a_platform_idle_forever();
}

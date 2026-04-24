#include "armv7a_context_smoke.hpp"

#include "armv7a_context_switch.hpp"
#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_kernel_port.hpp"
#include "armv7a_platform.hpp"

#include <array>
#include <cstdint>

namespace {
struct Armv7aContextSmokeArgument {
    std::uintptr_t* main_sp{nullptr};
    std::uint32_t cookie{0u};
};

constexpr std::uint32_t kArmv7aContextSmokeCookie = 0xC07ECAFEu;

alignas(8) std::array<std::uint8_t, 1024> g_armv7aContextSmokeStack{};
Armv7aContextSmokeArgument g_armv7aContextSmokeArgument{};
volatile bool g_armv7aContextSmokeEntrySeen = false;
volatile bool g_armv7aContextSmokeResumedSeen = false;
volatile bool g_armv7aContextSmokeUnexpectedReturn = false;
volatile bool g_armv7aContextSmokeRoundTrip = false;
std::uintptr_t g_armv7aContextSmokeMainSpSaved = 0u;
std::uintptr_t g_armv7aContextSmokeThreadSpSaved = 0u;
std::uintptr_t g_armv7aContextSmokeMainSpBefore = 0u;
std::uintptr_t g_armv7aContextSmokeThreadSpEntry = 0u;
std::uintptr_t g_armv7aContextSmokeThreadSpResume = 0u;

bool armv7a_context_sp_in_range(std::uintptr_t sp) noexcept
{
    const auto base =
        reinterpret_cast<std::uintptr_t>(g_armv7aContextSmokeStack.data());
    const auto top = base + g_armv7aContextSmokeStack.size();
    return sp >= base && sp < top;
}

extern "C" void armv7a_context_smoke_thread_entry(void* raw_argument)
{
    auto* argument = static_cast<Armv7aContextSmokeArgument*>(raw_argument);
    g_armv7aContextSmokeEntrySeen =
        argument != nullptr &&
        argument->main_sp != nullptr &&
        argument->cookie == kArmv7aContextSmokeCookie;
    g_armv7aContextSmokeThreadSpEntry = armv7a_read_sp();
    if (!g_armv7aContextSmokeEntrySeen) {
        armv7a_platform_early_console_puts(
            "ARMv7-A context switch smoke, bad thread argument\r\n");
        armv7a_platform_idle_forever();
    }

    (void)armv7a_context_switch(
        &g_armv7aContextSmokeThreadSpSaved, *argument->main_sp);

    g_armv7aContextSmokeResumedSeen = true;
    g_armv7aContextSmokeThreadSpResume = armv7a_read_sp();
    (void)armv7a_context_switch(
        &g_armv7aContextSmokeThreadSpSaved, *argument->main_sp);

    g_armv7aContextSmokeUnexpectedReturn = true;
    armv7a_platform_idle_forever();
}
} // namespace

void armv7a_run_context_switch_smoke()
{
    const auto contract = armv7a_make_qemu_kernel_port_contract();
    const auto stack_base =
        reinterpret_cast<std::uintptr_t>(g_armv7aContextSmokeStack.data());
    const auto stack_top = stack_base + g_armv7aContextSmokeStack.size();

    g_armv7aContextSmokeArgument = Armv7aContextSmokeArgument{
        .main_sp = &g_armv7aContextSmokeMainSpSaved,
        .cookie = kArmv7aContextSmokeCookie,
    };
    g_armv7aContextSmokeEntrySeen = false;
    g_armv7aContextSmokeResumedSeen = false;
    g_armv7aContextSmokeUnexpectedReturn = false;
    g_armv7aContextSmokeRoundTrip = false;
    g_armv7aContextSmokeMainSpSaved = 0u;
    g_armv7aContextSmokeThreadSpSaved = 0u;
    g_armv7aContextSmokeMainSpBefore = armv7a_read_sp();
    g_armv7aContextSmokeThreadSpEntry = 0u;
    g_armv7aContextSmokeThreadSpResume = 0u;

    const auto prepared_sp = armv7a_prepare_cooperative_thread_context(
        stack_top,
        reinterpret_cast<std::uintptr_t>(&armv7a_context_smoke_thread_entry),
        reinterpret_cast<std::uintptr_t>(&g_armv7aContextSmokeArgument));
    const auto frame = armv7a_make_thread_context_observation(
        stack_base,
        stack_top,
        prepared_sp,
        reinterpret_cast<std::uintptr_t>(&armv7a_context_smoke_thread_entry),
        reinterpret_cast<std::uintptr_t>(&g_armv7aContextSmokeArgument));
    armv7a_print_thread_context_frame(frame);

    const auto tick_runtime_ready =
        armv7a_kernel_tick_runtime_ready(contract);
    const auto thread_runtime_ready =
        armv7a_kernel_thread_runtime_ready(contract);

    const auto first_switch = thread_runtime_ready &&
        contract.context.switch_context(
            contract.context.ctx,
            &g_armv7aContextSmokeMainSpSaved,
            prepared_sp);
    const auto second_switch = first_switch &&
        contract.context.switch_context(
            contract.context.ctx,
            &g_armv7aContextSmokeMainSpSaved,
            g_armv7aContextSmokeThreadSpSaved);

    const auto round_trip_ok =
        tick_runtime_ready &&
        thread_runtime_ready &&
        armv7a_thread_context_frame_ready(frame) &&
        g_armv7aContextSmokeEntrySeen &&
        g_armv7aContextSmokeResumedSeen &&
        !g_armv7aContextSmokeUnexpectedReturn &&
        first_switch &&
        second_switch &&
        g_armv7aContextSmokeMainSpSaved != 0u &&
        g_armv7aContextSmokeThreadSpSaved != 0u &&
        armv7a_context_sp_in_range(g_armv7aContextSmokeThreadSpSaved) &&
        armv7a_context_sp_in_range(g_armv7aContextSmokeThreadSpEntry) &&
        armv7a_context_sp_in_range(g_armv7aContextSmokeThreadSpResume) &&
        g_armv7aContextSmokeMainSpBefore > g_armv7aContextSmokeMainSpSaved;
    g_armv7aContextSmokeRoundTrip = round_trip_ok;

    armv7a_platform_early_console_puts(
        "ARMv7-A context switch smoke, main-before=0x");
    armv7a_diag_put_hex(g_armv7aContextSmokeMainSpBefore);
    armv7a_platform_early_console_puts(", main-saved=0x");
    armv7a_diag_put_hex(g_armv7aContextSmokeMainSpSaved);
    armv7a_platform_early_console_puts(", thread-entry-sp=0x");
    armv7a_diag_put_hex(g_armv7aContextSmokeThreadSpEntry);
    armv7a_platform_early_console_puts(", thread-saved=0x");
    armv7a_diag_put_hex(g_armv7aContextSmokeThreadSpSaved);
    armv7a_platform_early_console_puts(", thread-resume-sp=0x");
    armv7a_diag_put_hex(g_armv7aContextSmokeThreadSpResume);
    armv7a_platform_early_console_puts(", entry=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(g_armv7aContextSmokeEntrySeen));
    armv7a_platform_early_console_puts(", resumed=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(g_armv7aContextSmokeResumedSeen));
    armv7a_platform_early_console_puts(", round-trip=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(round_trip_ok));
    armv7a_platform_early_console_puts("\r\n");
}

Armv7aContextSwitchSmokeObservation
armv7a_context_switch_smoke_last_observation() noexcept
{
    return Armv7aContextSwitchSmokeObservation{
        .entry_seen = g_armv7aContextSmokeEntrySeen,
        .resumed_seen = g_armv7aContextSmokeResumedSeen,
        .unexpected_return = g_armv7aContextSmokeUnexpectedReturn,
        .round_trip = g_armv7aContextSmokeRoundTrip,
        .main_sp_before = g_armv7aContextSmokeMainSpBefore,
        .main_sp_saved = g_armv7aContextSmokeMainSpSaved,
        .thread_entry_sp = g_armv7aContextSmokeThreadSpEntry,
        .thread_saved_sp = g_armv7aContextSmokeThreadSpSaved,
        .thread_resume_sp = g_armv7aContextSmokeThreadSpResume,
    };
}

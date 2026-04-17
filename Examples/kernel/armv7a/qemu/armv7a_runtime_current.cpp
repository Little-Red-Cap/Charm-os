#include "armv7a_runtime_current.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"

namespace {
constexpr std::uint64_t kArmv7aRuntimeCurrentTask = 0x0000000013572468ull;
constexpr std::uint64_t kArmv7aRuntimeCurrentStack = 0x0000000052001000ull;

struct Armv7aRuntimeCurrentSlot {
    bool seen = false;
    Armv7aRuntimeCurrentContext current{};
};

Armv7aRuntimeCurrentContextPort g_runtime_current_context_port{};
Armv7aRuntimeCurrentSlot g_runtime_current_slot{};
Armv7aRuntimeCurrentSlot g_runtime_current_sample{};

bool armv7a_capture_runtime_current_from_slot(
    void*,
    Armv7aRuntimeCurrentContext& out) noexcept
{
    if (!g_runtime_current_slot.seen) {
        out = {};
        return false;
    }

    out = g_runtime_current_slot.current;
    return true;
}

Armv7aRuntimeCurrentContextPort armv7a_default_runtime_current_context_port()
    noexcept
{
    return Armv7aRuntimeCurrentContextPort{
        .ctx = nullptr,
        .capture = armv7a_capture_runtime_current_from_slot,
    };
}

void armv7a_record_runtime_current_sample(
    Armv7aRuntimeCurrentContext current) noexcept
{
    g_runtime_current_sample = Armv7aRuntimeCurrentSlot{
        .seen = true,
        .current = current,
    };
}
} // namespace

Armv7aRuntimeCurrentContextPort armv7a_runtime_current_context_port() noexcept
{
    return armv7a_runtime_current_context_port_ready(g_runtime_current_context_port)
        ? g_runtime_current_context_port
        : armv7a_default_runtime_current_context_port();
}

void armv7a_bind_runtime_current_context_port(
    Armv7aRuntimeCurrentContextPort port) noexcept
{
    g_runtime_current_context_port = port;
}

void armv7a_unbind_runtime_current_context_port() noexcept
{
    g_runtime_current_context_port = {};
}

void armv7a_publish_runtime_current_context(
    Armv7aRuntimeCurrentContext current) noexcept
{
    g_runtime_current_slot = Armv7aRuntimeCurrentSlot{
        .seen = true,
        .current = current,
    };
}

void armv7a_publish_runtime_current_here(std::uint64_t task) noexcept
{
    const auto current = Armv7aRuntimeCurrentContext{
        .stack_pointer = armv7a_read_sp(),
        .task = task,
        .task_valid = true,
    };
    armv7a_publish_runtime_current_context(current);
    armv7a_record_runtime_current_sample(current);
}

void armv7a_clear_runtime_current_context() noexcept
{
    g_runtime_current_slot = {};
}

Armv7aRuntimeCurrentContext armv7a_capture_runtime_current_context() noexcept
{
    Armv7aRuntimeCurrentContext current{};
    (void)armv7a_runtime_current_context_port_capture(
        armv7a_runtime_current_context_port(), current);
    return current;
}

bool armv7a_capture_runtime_current_sample_context(
    Armv7aRuntimeCurrentContext& out) noexcept
{
    if (!g_runtime_current_sample.seen) {
        out = {};
        return false;
    }

    out = g_runtime_current_sample.current;
    return true;
}

Armv7aRuntimeCurrentObservation
armv7a_capture_runtime_current_observation() noexcept
{
    const auto expected = Armv7aRuntimeCurrentContext{
        .stack_pointer = kArmv7aRuntimeCurrentStack,
        .task = kArmv7aRuntimeCurrentTask,
        .task_valid = true,
    };
    armv7a_publish_runtime_current_context(expected);

    Armv7aRuntimeCurrentContext current{};
    const auto port = armv7a_runtime_current_context_port();
    const auto current_seen =
        armv7a_runtime_current_context_port_capture(port, current);
    const auto ingress = armv7a_make_runtime_trap_ingress_context(current);
    const auto observation = Armv7aRuntimeCurrentObservation{
        .current = current,
        .ingress = ingress,
        .path = current_seen &&
                armv7a_runtime_current_context_port_ready(port)
            ? Armv7aRuntimeCurrentPath::current_slot
            : Armv7aRuntimeCurrentPath::none,
        .port_ready = armv7a_runtime_current_context_port_ready(port),
        .current_seen = current_seen,
        .task_matches = current.task == expected.task &&
                        current.task_valid == expected.task_valid,
        .stack_matches = current.stack_pointer == expected.stack_pointer,
        .ingress_matches = ingress.stack_pointer == expected.stack_pointer &&
                           ingress.task == expected.task &&
                           ingress.task_valid == expected.task_valid,
    };

    armv7a_clear_runtime_current_context();
    return observation;
}

void armv7a_print_runtime_current_observation()
{
    const auto observation = armv7a_capture_runtime_current_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime current, path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_current_path_name(observation.path));
    armv7a_platform_early_console_puts(", task=0x");
    armv7a_diag_put_hex64(observation.current.task, 16);
    armv7a_platform_early_console_puts(", sp=0x");
    armv7a_diag_put_hex64(observation.current.stack_pointer, 16);
    armv7a_platform_early_console_puts(", task-valid=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        observation.current.task_valid));
    armv7a_platform_early_console_puts(", current=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_current_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

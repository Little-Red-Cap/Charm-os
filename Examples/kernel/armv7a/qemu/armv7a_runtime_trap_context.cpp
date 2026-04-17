#include "armv7a_runtime_trap_context.hpp"

#include "armv7a_cpu.hpp"
#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_bridge_contract.hpp"

namespace {
constexpr std::uint64_t kArmv7aRuntimeTrapContextTask = 0x0000000013572468ull;
constexpr std::uint64_t kArmv7aRuntimeTrapContextStack = 0x0000000052001000ull;
constexpr std::uint64_t kArmv7aRuntimeTrapContextYieldValue = 1u;
constexpr std::uint64_t kArmv7aRuntimeTrapContextSleepValue = 5u;

Armv7aRuntimeTrapContextPort g_runtime_trap_context_port{};

bool armv7a_qemu_runtime_trap_context_stub(
    void*,
    Armv7aRuntimeTrapIngressContext& out) noexcept
{
    out = Armv7aRuntimeTrapIngressContext{
        .stack_pointer = kArmv7aRuntimeTrapContextStack,
        .task = kArmv7aRuntimeTrapContextTask,
        .task_valid = true,
    };
    return true;
}

Armv7aRuntimeTrapContextPort armv7a_default_runtime_trap_context_port() noexcept
{
    return Armv7aRuntimeTrapContextPort{
        .ctx = nullptr,
        .capture = nullptr,
    };
}

Armv7aRuntimeTrapRoundtripObservation armv7a_make_runtime_trap_context_roundtrip(
    std::uint32_t service_id,
    std::uint64_t expected_value,
    std::uint32_t return_value,
    const Armv7aRuntimeTrapDispatchObservation& dispatch) noexcept
{
    return Armv7aRuntimeTrapRoundtripObservation{
        .result = dispatch.result,
        .service_id = service_id,
        .return_value = return_value,
        .expected_value = expected_value,
        .path = Armv7aRuntimeTrapRoundtripPath::svc_return,
        .service_ready = true,
        .value_fits_return_register =
            armv7a_runtime_trap_roundtrip_value_fits_return_register(
                dispatch.result.value),
        .return_matches_result =
            return_value == static_cast<std::uint32_t>(dispatch.result.value),
        .return_matches_expected =
            return_value == static_cast<std::uint32_t>(expected_value),
    };
}

Armv7aRuntimeTrapContextProbeObservation armv7a_make_runtime_trap_context_probe(
    const Armv7aRuntimeTrapDispatchObservation& dispatch,
    const Armv7aRuntimeTrapIngressContext& expected,
    const Armv7aRuntimeTrapRoundtripObservation& roundtrip,
    bool port_ready) noexcept
{
    return Armv7aRuntimeTrapContextProbeObservation{
        .dispatch = dispatch,
        .roundtrip = roundtrip,
        .expected = expected,
        .path = port_ready
            ? Armv7aRuntimeTrapContextPath::context_port
            : Armv7aRuntimeTrapContextPath::none,
        .port_ready = port_ready,
        .context_seen = dispatch.frame_view.task_valid,
        .task_matches =
            dispatch.frame_view.task == expected.task &&
            dispatch.frame_view.task_valid == expected.task_valid,
        .stack_matches = dispatch.frame_view.stack_pointer == expected.stack_pointer,
        .roundtrip_matches_dispatch =
            dispatch.result_register_after == roundtrip.return_value,
    };
}
} // namespace

Armv7aRuntimeTrapContextPort armv7a_runtime_trap_context_port() noexcept
{
    return armv7a_runtime_trap_context_port_ready(g_runtime_trap_context_port)
        ? g_runtime_trap_context_port
        : armv7a_default_runtime_trap_context_port();
}

void armv7a_bind_runtime_trap_context_port(
    Armv7aRuntimeTrapContextPort port) noexcept
{
    g_runtime_trap_context_port = port;
}

void armv7a_unbind_runtime_trap_context_port() noexcept
{
    g_runtime_trap_context_port = {};
}

Armv7aRuntimeTrapIngressContext armv7a_capture_runtime_trap_ingress_context()
    noexcept
{
    Armv7aRuntimeTrapIngressContext context{};
    (void)armv7a_runtime_trap_context_port_capture(
        armv7a_runtime_trap_context_port(), context);
    return context;
}

Armv7aRuntimeTrapContextPairObservation
armv7a_capture_runtime_trap_context_observation() noexcept
{
    const auto context_port = Armv7aRuntimeTrapContextPort{
        .ctx = nullptr,
        .capture = armv7a_qemu_runtime_trap_context_stub,
    };
    armv7a_bind_runtime_trap_context_port(context_port);

    const auto expected = armv7a_capture_runtime_trap_ingress_context();
    const auto yield_return = armv7a_svc_smoke_test_result();
    const auto sleep_return = armv7a_svc_sleep_smoke_test_result();
    const auto dispatch = armv7a_capture_runtime_trap_dispatch_observation();
    const auto port_ready = armv7a_runtime_trap_context_port_ready(
        armv7a_runtime_trap_context_port());

    const auto observation = Armv7aRuntimeTrapContextPairObservation{
        .yield = armv7a_make_runtime_trap_context_probe(
            dispatch.yield,
            expected,
            armv7a_make_runtime_trap_context_roundtrip(
                kArmv7aRuntimeBridgeYieldServiceId,
                kArmv7aRuntimeTrapContextYieldValue,
                yield_return,
                dispatch.yield),
            port_ready),
        .sleep = armv7a_make_runtime_trap_context_probe(
            dispatch.sleep,
            expected,
            armv7a_make_runtime_trap_context_roundtrip(
                kArmv7aRuntimeBridgeSleepServiceId,
                kArmv7aRuntimeTrapContextSleepValue,
                sleep_return,
                dispatch.sleep),
            port_ready),
    };

    armv7a_unbind_runtime_trap_context_port();
    return observation;
}

void armv7a_print_runtime_trap_context_observation()
{
    const auto observation = armv7a_capture_runtime_trap_context_observation();

    armv7a_platform_early_console_puts("ARMv7-A runtime trap context, yield-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_context_path_name(observation.yield.path));
    armv7a_platform_early_console_puts(", yield-task=0x");
    armv7a_diag_put_hex64(observation.yield.dispatch.frame_view.task, 16);
    armv7a_platform_early_console_puts(", yield-sp=0x");
    armv7a_diag_put_hex64(observation.yield.dispatch.frame_view.stack_pointer, 16);
    armv7a_platform_early_console_puts(", yield-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_context_probe_ready(observation.yield)));
    armv7a_platform_early_console_puts(", sleep-path=");
    armv7a_platform_early_console_puts(
        armv7a_runtime_trap_context_path_name(observation.sleep.path));
    armv7a_platform_early_console_puts(", sleep-task=0x");
    armv7a_diag_put_hex64(observation.sleep.dispatch.frame_view.task, 16);
    armv7a_platform_early_console_puts(", sleep-sp=0x");
    armv7a_diag_put_hex64(observation.sleep.dispatch.frame_view.stack_pointer, 16);
    armv7a_platform_early_console_puts(", sleep-ready=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_context_probe_ready(observation.sleep)));
    armv7a_platform_early_console_puts(", context=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_context_observation_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

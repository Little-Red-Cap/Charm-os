#include "armv7a_runtime_trap_failure.hpp"

#include <cstdint>

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_trap_dispatch.hpp"
#include "armv7a_runtime_trap_mapping.hpp"
#include "targets/armv7a/common/armv7a_exception_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_current_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_frame_adapter_contract.hpp"

namespace {
constexpr std::uint32_t kUsrMode = 0x10u;
constexpr std::uint32_t kSvcMode = 0x13u;
constexpr std::uint64_t kTask = 0x0000000052536001ull;
constexpr std::uint64_t kStack = 0x000000005200A000ull;

struct SyntheticLiveFrame {
    Armv7aExceptionFrame frame{};
    Armv7aRuntimeTrapLiveFrame live{};
};

[[nodiscard]] constexpr const char* ingress_error_name(
    Armv7aRuntimeTrapIngressError error) noexcept
{
    switch (error) {
    case Armv7aRuntimeTrapIngressError::none:
        return "none";
    case Armv7aRuntimeTrapIngressError::decode_failed:
        return "decode-failed";
    case Armv7aRuntimeTrapIngressError::writeback_failed:
        return "writeback-failed";
    case Armv7aRuntimeTrapIngressError::unsupported_service:
        return "unsupported-service";
    case Armv7aRuntimeTrapIngressError::unbound_adapter:
        return "unbound-adapter";
    }
    return "unknown";
}

[[nodiscard]] constexpr Armv7aRuntimeTrapFrameAdapterContext
make_lower_context() noexcept
{
    return Armv7aRuntimeTrapFrameAdapterContext{
        .policy = armv7a_qemu_runtime_trap_mapping_policy(),
        .ingress = armv7a_make_runtime_trap_ingress_context(
            Armv7aRuntimeCurrentContext{
                .stack_pointer = kStack,
                .task = kTask,
                .task_valid = true,
            }),
    };
}

void bind_svc_live_frame(SyntheticLiveFrame& out,
                         std::uint32_t service_id,
                         std::uint32_t arg0,
                         std::uint32_t arg1,
                         std::uint32_t arg2,
                         std::uint32_t arg3,
                         std::uint32_t origin_psr,
                         std::uint32_t return_pc) noexcept
{
    out.frame = Armv7aExceptionFrame{
        .spsr = origin_psr,
        .vector_id = kArmv7aExceptionSvc,
        .r0 = arg0,
        .r1 = arg1,
        .r2 = arg2,
        .r3 = arg3,
        .r12 = 0u,
        .lr = return_pc,
    };
    out.live = armv7a_make_runtime_trap_live_frame(
        out.frame,
        kSvcMode,
        0xef000000u | (service_id & 0x00ffffffu));
}

bool reject_apply_result(void*,
                         Armv7aRuntimeTrapLiveFrame&,
                         const Armv7aRuntimeTrapIngressResult&) noexcept
{
    return false;
}

[[nodiscard]] Armv7aRuntimeTrapIngressResult probe_unsupported_service() noexcept
{
    return armv7a_runtime_trap_dispatch_port_dispatch(
        armv7a_runtime_trap_dispatch_port(),
        Armv7aRuntimeTrapSeamFrameView{
            .service_id = 0x00FFu,
            .arg0 = 0x1234u,
            .origin = Armv7aRuntimeTrapOrigin::kernel_thread,
            .task = kTask,
            .task_valid = true,
        });
}

[[nodiscard]] Armv7aRuntimeTrapIngressResult probe_decode_failed() noexcept
{
    auto lower_context = make_lower_context();
    auto adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);
    auto port = armv7a_runtime_trap_dispatch_port();

    SyntheticLiveFrame invalid_frame{};
    bind_svc_live_frame(invalid_frame,
                        kArmv7aRuntimeBridgeYieldServiceId,
                        1u,
                        1u,
                        0u,
                        0u,
                        0u,
                        0x8600u);
    return armv7a_runtime_trap_dispatch_live_frame(
        invalid_frame.live, adapter, port);
}

[[nodiscard]] Armv7aRuntimeTrapIngressResult probe_writeback_failed() noexcept
{
    auto lower_context = make_lower_context();
    auto port = armv7a_runtime_trap_dispatch_port();
    const auto failing_adapter = Armv7aRuntimeTrapFrameAdapter{
        .ctx = &lower_context,
        .capture = &armv7a_runtime_trap_frame_adapter_capture,
        .apply_result = &reject_apply_result,
    };

    SyntheticLiveFrame yield_frame{};
    bind_svc_live_frame(yield_frame,
                        kArmv7aRuntimeBridgeYieldServiceId,
                        1u,
                        1u,
                        0u,
                        0u,
                        kUsrMode,
                        0x8604u);
    return armv7a_runtime_trap_dispatch_live_frame(
        yield_frame.live, failing_adapter, port);
}

[[nodiscard]] Armv7aRuntimeTrapIngressResult probe_unbound_adapter() noexcept
{
    auto port = armv7a_runtime_trap_dispatch_port();

    SyntheticLiveFrame yield_frame{};
    bind_svc_live_frame(yield_frame,
                        kArmv7aRuntimeBridgeYieldServiceId,
                        1u,
                        1u,
                        0u,
                        0u,
                        kUsrMode,
                        0x8608u);
    return armv7a_runtime_trap_dispatch_live_frame(
        yield_frame.live, Armv7aRuntimeTrapFrameAdapter{}, port);
}

[[nodiscard]] Armv7aRuntimeTrapIngressResult probe_unbound_dispatch() noexcept
{
    auto lower_context = make_lower_context();
    auto adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);

    SyntheticLiveFrame yield_frame{};
    bind_svc_live_frame(yield_frame,
                        kArmv7aRuntimeBridgeYieldServiceId,
                        1u,
                        1u,
                        0u,
                        0u,
                        kUsrMode,
                        0x860Cu);
    return armv7a_runtime_trap_dispatch_live_frame(
        yield_frame.live, adapter, Armv7aRuntimeTrapDispatchPort{});
}
} // namespace

Armv7aRuntimeTrapFailureObservation
armv7a_capture_runtime_trap_failure_observation() noexcept
{
    const auto unsupported = probe_unsupported_service();
    const auto decode = probe_decode_failed();
    const auto writeback = probe_writeback_failed();
    const auto adapter = probe_unbound_adapter();
    const auto dispatch = probe_unbound_dispatch();

    return Armv7aRuntimeTrapFailureObservation{
        .unsupported_error = ingress_error_name(unsupported.error),
        .decode_error = ingress_error_name(decode.error),
        .writeback_error = ingress_error_name(writeback.error),
        .adapter_error = ingress_error_name(adapter.error),
        .dispatch_error = ingress_error_name(dispatch.error),
        .unsupported_ready =
            unsupported.disposition ==
                Armv7aRuntimeTrapIngressDisposition::unsupported &&
            unsupported.error ==
                Armv7aRuntimeTrapIngressError::unsupported_service,
        .decode_ready =
            decode.disposition ==
                Armv7aRuntimeTrapIngressDisposition::rejected &&
            decode.error == Armv7aRuntimeTrapIngressError::decode_failed,
        .writeback_ready =
            writeback.disposition ==
                Armv7aRuntimeTrapIngressDisposition::rejected &&
            writeback.error ==
                Armv7aRuntimeTrapIngressError::writeback_failed &&
            writeback.value == 1u,
        .adapter_ready =
            adapter.disposition ==
                Armv7aRuntimeTrapIngressDisposition::rejected &&
            adapter.error == Armv7aRuntimeTrapIngressError::unbound_adapter,
        .dispatch_ready =
            dispatch.disposition ==
                Armv7aRuntimeTrapIngressDisposition::rejected &&
            dispatch.error == Armv7aRuntimeTrapIngressError::unbound_adapter,
    };
}

void armv7a_print_runtime_trap_failure_observation()
{
    const auto observation = armv7a_capture_runtime_trap_failure_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A runtime trap failure, unsupported=");
    armv7a_platform_early_console_puts(observation.unsupported_error);
    armv7a_platform_early_console_puts(", decode=");
    armv7a_platform_early_console_puts(observation.decode_error);
    armv7a_platform_early_console_puts(", writeback=");
    armv7a_platform_early_console_puts(observation.writeback_error);
    armv7a_platform_early_console_puts(", adapter=");
    armv7a_platform_early_console_puts(observation.adapter_error);
    armv7a_platform_early_console_puts(", dispatch=");
    armv7a_platform_early_console_puts(observation.dispatch_error);
    armv7a_platform_early_console_puts(", failure=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_runtime_trap_failure_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

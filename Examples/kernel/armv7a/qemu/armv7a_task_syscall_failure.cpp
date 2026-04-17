#include "armv7a_task_syscall_failure.hpp"

#include <array>
#include <cstdint>

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "targets/armv7a/common/armv7a_exception_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_bridge_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_current_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_caller_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_frame_adapter_contract.hpp"

import kernel.task_syscall_frame;
import target.armv7a.kernel_runtime_trap_frame_adapter;
import target.armv7a.kernel_task_syscall_call_frame_adapter;

namespace {
constexpr std::uint32_t kUsrMode = 0x10u;
constexpr std::uint32_t kSvcMode = 0x13u;
constexpr std::uint32_t kSysMode = 0x1fu;
constexpr std::uint64_t kTask = 0x0000000059535001ull;
constexpr std::uint64_t kStack = 0x0000000052009000ull;
constexpr std::uint64_t kDebugValue = 0x000000CCull;
constexpr std::uint64_t kCapabilityId = 0x00000007ull;
constexpr std::uint64_t kCapabilityOperation = 0x00000002ull;
constexpr std::uint64_t kCapabilityPayload = 0x00000021ull;

struct DebugHandlerState {
    std::uint32_t calls{0};
    std::uint64_t last_value{0};
};

struct DebugHandler {
    DebugHandlerState* state{nullptr};

    [[nodiscard]] kernel::TrapResult dispatch(
        kernel::TaskSyscallRequest request) const noexcept
    {
        if (state == nullptr) {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::unbound_adapter,
                .value = 0u,
            };
        }

        if (request.syscall != kernel::TaskSyscallId::debug_write) {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::invalid_argument,
                .value = 0u,
            };
        }

        ++state->calls;
        state->last_value = request.arg0;
        return kernel::TrapResult{
            .disposition = kernel::TrapDisposition::handled,
            .error = kernel::TrapError::none,
            .value = request.arg0 + 1u,
        };
    }
};

using TableTrace = kernel::TaskSyscallTableTraceBuffer<8>;
using FrameTrace = kernel::TaskSyscallFrameTraceBuffer<8>;
using DebugTable = kernel::TaskSyscallTable<1, TableTrace>;
using BridgeType =
    kernel::TaskSyscallFrameBridge<DebugTable, Armv7aRuntimeTrapLiveFrame, FrameTrace>;

struct SyntheticLiveFrame {
    Armv7aExceptionFrame frame{};
    Armv7aRuntimeTrapLiveFrame live{};
};

struct DebugCallState {
    Armv7aRuntimeTrapCallContext context{};
    SyntheticLiveFrame scratch{};
};

[[nodiscard]] constexpr Armv7aRuntimeTrapFrameAdapterContext
make_lower_context() noexcept
{
    return Armv7aRuntimeTrapFrameAdapterContext{
        .policy = {},
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

[[nodiscard]] auto make_debug_table(DebugHandler& debug_handler,
                                    TableTrace* trace) noexcept -> DebugTable
{
    return kernel::make_task_syscall_table(
        std::array<kernel::TaskSyscallHandlerEntry, 1>{
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::debug_write,
                kernel::make_task_syscall_handler(debug_handler)),
        },
        trace);
}

bool make_debug_call_frame(void* ctx,
                           kernel::TrapDebugWriteView write,
                           Armv7aRuntimeTrapLiveFrame& out) noexcept
{
    auto* state = static_cast<DebugCallState*>(ctx);
    if (state == nullptr) {
        return false;
    }

    bind_svc_live_frame(state->scratch,
                        kArmv7aRuntimeBridgeDebugWriteServiceId,
                        static_cast<std::uint32_t>(write.value),
                        0u,
                        0u,
                        0u,
                        state->context.origin_psr,
                        state->context.return_pc);
    out = state->scratch.live;
    return true;
}

bool always_writeback_fail(void*,
                           const Armv7aRuntimeTrapLiveFrame&,
                           const kernel::TrapResult&) noexcept
{
    return false;
}

bool unused_yield_frame(void*,
                        kernel::TrapYieldCurrentView,
                        Armv7aRuntimeTrapLiveFrame&) noexcept
{
    return false;
}

bool unused_sleep_frame(void*,
                        kernel::TrapSleepUntilView<std::uint64_t>,
                        Armv7aRuntimeTrapLiveFrame&) noexcept
{
    return false;
}

[[nodiscard]] kernel::TrapResult probe_decode_failed() noexcept
{
    DebugHandlerState debug_state{};
    DebugHandler debug_handler{.state = &debug_state};
    TableTrace table_trace{};
    FrameTrace frame_trace{};
    auto table = make_debug_table(debug_handler, &table_trace);

    auto lower_context = make_lower_context();
    auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);
    Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
        .lower = lower_adapter,
    };
    auto bridge = armv7a_make_task_syscall_frame_bridge(
        table, wrapper, &frame_trace);

    SyntheticLiveFrame invalid_frame{};
    bind_svc_live_frame(invalid_frame,
                        kArmv7aRuntimeBridgeDebugWriteServiceId,
                        0xEEu,
                        0u,
                        0u,
                        0u,
                        0u,
                        0x8500u);
    return bridge.dispatch(invalid_frame.live);
}

[[nodiscard]] kernel::TrapResult probe_unsupported_service() noexcept
{
    DebugHandlerState debug_state{};
    DebugHandler debug_handler{.state = &debug_state};
    TableTrace table_trace{};
    FrameTrace frame_trace{};
    auto table = make_debug_table(debug_handler, &table_trace);

    auto lower_context = make_lower_context();
    auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);
    Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
        .lower = lower_adapter,
    };
    auto bridge = armv7a_make_task_syscall_frame_bridge(
        table, wrapper, &frame_trace);

    SyntheticLiveFrame capability_frame{};
    bind_svc_live_frame(capability_frame,
                        kArmv7aRuntimeBridgeCapabilityCallServiceId,
                        static_cast<std::uint32_t>(kCapabilityId),
                        static_cast<std::uint32_t>(kCapabilityOperation),
                        static_cast<std::uint32_t>(kCapabilityPayload),
                        0u,
                        kUsrMode,
                        0x8504u);
    return bridge.dispatch(capability_frame.live);
}

[[nodiscard]] kernel::TrapResult probe_unbound_bridge() noexcept
{
    auto lower_context = make_lower_context();
    auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);
    Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
        .lower = lower_adapter,
    };
    auto runtime_adapter = armv7a_make_kernel_runtime_trap_frame_adapter(wrapper);
    BridgeType bridge{};
    bridge.bind_adapter(kernel::make_task_syscall_frame_adapter(runtime_adapter));

    SyntheticLiveFrame debug_frame{};
    bind_svc_live_frame(debug_frame,
                        kArmv7aRuntimeBridgeDebugWriteServiceId,
                        static_cast<std::uint32_t>(kDebugValue),
                        0u,
                        0u,
                        0u,
                        kSysMode,
                        0x8508u);
    return bridge.dispatch(debug_frame.live);
}

[[nodiscard]] kernel::TrapResult probe_unbound_caller() noexcept
{
    const auto caller =
        kernel::TaskSyscallFrameCaller<Armv7aRuntimeTrapLiveFrame, std::uint64_t>{};
    return caller.yield();
}

[[nodiscard]] kernel::TrapResult probe_writeback_failed() noexcept
{
    DebugHandlerState debug_state{};
    DebugHandler debug_handler{.state = &debug_state};
    TableTrace table_trace{};
    FrameTrace frame_trace{};
    auto table = make_debug_table(debug_handler, &table_trace);

    auto lower_context = make_lower_context();
    auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);
    Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
        .lower = lower_adapter,
    };
    auto frame_bridge = armv7a_make_task_syscall_frame_bridge(
        table, wrapper, &frame_trace);

    DebugCallState state{
        .context =
            Armv7aRuntimeTrapCallContext{
                .origin_psr = kSysMode,
                .handler_psr = kSvcMode,
                .return_pc = 0x8510u,
                .stack_pointer = kStack,
                .task = kTask,
                .task_valid = true,
            },
    };
    auto runtime_call_adapter =
        kernel::RuntimeTrapCallFrameAdapter<Armv7aRuntimeTrapLiveFrame,
                                            std::uint64_t>{
            .ctx = &state,
            .make_yield_frame = &unused_yield_frame,
            .make_sleep_frame = &unused_sleep_frame,
            .make_debug_write_frame = &make_debug_call_frame,
            .result_ready = &always_writeback_fail,
        };
    auto caller = armv7a_make_task_syscall_frame_caller(
        frame_bridge.port(), runtime_call_adapter);
    return caller.debug_write(kDebugValue);
}
} // namespace

Armv7aTaskSyscallFailureObservation
armv7a_capture_task_syscall_failure_observation() noexcept
{
    const auto decode = probe_decode_failed();
    const auto unsupported = probe_unsupported_service();
    const auto unbound_bridge = probe_unbound_bridge();
    const auto unbound_caller = probe_unbound_caller();
    const auto writeback = probe_writeback_failed();

    return Armv7aTaskSyscallFailureObservation{
        .decode_error = kernel::trap_error_name(decode.error),
        .unsupported_error = kernel::trap_error_name(unsupported.error),
        .unbound_bridge_error = kernel::trap_error_name(unbound_bridge.error),
        .unbound_caller_error = kernel::trap_error_name(unbound_caller.error),
        .writeback_error = kernel::trap_error_name(writeback.error),
        .decode_ready =
            decode.disposition == kernel::TrapDisposition::rejected &&
            decode.error == kernel::TrapError::decode_failed,
        .unsupported_ready =
            unsupported.disposition == kernel::TrapDisposition::unsupported &&
            unsupported.error == kernel::TrapError::unsupported_service,
        .unbound_bridge_ready =
            unbound_bridge.disposition == kernel::TrapDisposition::rejected &&
            unbound_bridge.error == kernel::TrapError::unbound_bridge,
        .unbound_caller_ready =
            unbound_caller.disposition == kernel::TrapDisposition::rejected &&
            unbound_caller.error == kernel::TrapError::unbound_adapter,
        .writeback_ready =
            writeback.disposition == kernel::TrapDisposition::rejected &&
            writeback.error == kernel::TrapError::writeback_failed,
    };
}

void armv7a_print_task_syscall_failure_observation()
{
    const auto observation = armv7a_capture_task_syscall_failure_observation();

    armv7a_platform_early_console_puts(
        "ARMv7-A task syscall failure, decode=");
    armv7a_platform_early_console_puts(observation.decode_error);
    armv7a_platform_early_console_puts(", unsupported=");
    armv7a_platform_early_console_puts(observation.unsupported_error);
    armv7a_platform_early_console_puts(", bridge=");
    armv7a_platform_early_console_puts(observation.unbound_bridge_error);
    armv7a_platform_early_console_puts(", caller=");
    armv7a_platform_early_console_puts(observation.unbound_caller_error);
    armv7a_platform_early_console_puts(", writeback=");
    armv7a_platform_early_console_puts(observation.writeback_error);
    armv7a_platform_early_console_puts(", failure=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_failure_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

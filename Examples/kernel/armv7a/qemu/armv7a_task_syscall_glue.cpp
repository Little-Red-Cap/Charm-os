#include "armv7a_task_syscall_glue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include "armv7a_diag_console.hpp"
#include "armv7a_platform.hpp"
#include "armv7a_runtime_trap_mapping.hpp"
#include "targets/armv7a/common/armv7a_exception_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_bridge_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_current_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_caller_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_frame_adapter_contract.hpp"

import kernel.task_syscall_frame;
import kernel.task_runtime_api;
import target.armv7a.kernel_runtime_trap_frame_adapter;
import target.armv7a.kernel_task_syscall_call_frame_adapter;

namespace {
constexpr std::uint32_t kUsrMode = 0x10u;
constexpr std::uint32_t kSvcMode = 0x13u;
constexpr std::uint32_t kSysMode = 0x1fu;
constexpr std::uint64_t kTask = 0x0000000059534001ull;
constexpr std::uint64_t kStack = 0x0000000052008000ull;
constexpr std::uint64_t kSleepDue = 55u;
constexpr std::uint64_t kDebugValue = 0x000000CCull;
constexpr std::uint64_t kCapabilityId = 0x00000007ull;
constexpr std::uint64_t kCapabilityOperation = 0x00000002ull;
constexpr std::uint64_t kCapabilityPayload = 0x00000021ull;
constexpr std::uint64_t kCapabilityResult = 0x0000002Aull;
constexpr std::uint64_t kRuntimeApiSleepDue = 63u;
constexpr std::uint64_t kRuntimeApiDebugValue = 0x000000CDull;
constexpr std::uint64_t kRuntimeApiCapabilityId = 0x00000008ull;
constexpr std::uint64_t kRuntimeApiCapabilityOperation = 0x00000003ull;
constexpr std::uint64_t kRuntimeApiCapabilityPayload = 0x00000020ull;
constexpr std::uint64_t kRuntimeApiCapabilityResult = 0x0000002Bull;

struct DebugState {
    std::uint32_t calls{0};
    std::uint64_t last_value{0};
};

struct YieldState {
    std::uint32_t calls{0};
};

struct SleepState {
    std::uint32_t calls{0};
    std::uint64_t last_due{0};
};

struct CapabilityState {
    std::uint32_t calls{0};
    std::uint64_t last_id{0};
    std::uint64_t last_operation{0};
    std::uint64_t last_payload{0};
};

[[nodiscard]] constexpr kernel::TrapResult handled(std::uint64_t value) noexcept
{
    return kernel::TrapResult{
        .disposition = kernel::TrapDisposition::handled,
        .error = kernel::TrapError::none,
        .value = value,
    };
}

[[nodiscard]] constexpr kernel::TrapResult rejected(
    kernel::TrapError error) noexcept
{
    return kernel::TrapResult{
        .disposition = kernel::TrapDisposition::rejected,
        .error = error,
        .value = 0u,
    };
}

[[nodiscard]] constexpr bool result_matches(const kernel::TrapResult& result,
                                            kernel::TrapDisposition disposition,
                                            kernel::TrapError error,
                                            std::uint64_t value) noexcept
{
    return result.disposition == disposition && result.error == error &&
           result.value == value;
}

struct DebugHandler {
    DebugState* state{nullptr};

    [[nodiscard]] kernel::TrapResult dispatch(
        kernel::TaskSyscallRequest request) const noexcept
    {
        if (state == nullptr) {
            return rejected(kernel::TrapError::unbound_adapter);
        }
        if (request.syscall != kernel::TaskSyscallId::debug_write) {
            return rejected(kernel::TrapError::invalid_argument);
        }
        ++state->calls;
        state->last_value = request.arg0;
        return handled(request.arg0 + 1u);
    }
};

struct YieldHandler {
    YieldState* state{nullptr};

    [[nodiscard]] kernel::TrapResult dispatch(
        kernel::TaskSyscallRequest request) const noexcept
    {
        if (state == nullptr) {
            return rejected(kernel::TrapError::unbound_adapter);
        }
        if (request.syscall != kernel::TaskSyscallId::yield) {
            return rejected(kernel::TrapError::invalid_argument);
        }
        ++state->calls;
        return handled(1u);
    }
};

struct SleepHandler {
    SleepState* state{nullptr};

    [[nodiscard]] kernel::TrapResult dispatch(
        kernel::TaskSyscallRequest request) const noexcept
    {
        if (state == nullptr) {
            return rejected(kernel::TrapError::unbound_adapter);
        }
        if (request.syscall != kernel::TaskSyscallId::sleep_until) {
            return rejected(kernel::TrapError::invalid_argument);
        }
        ++state->calls;
        state->last_due = request.arg0;
        return handled(request.arg0);
    }
};

struct CapabilityHandler {
    CapabilityState* state{nullptr};

    [[nodiscard]] kernel::TrapResult dispatch(
        kernel::TaskSyscallRequest request) const noexcept
    {
        if (state == nullptr) {
            return rejected(kernel::TrapError::unbound_adapter);
        }
        if (request.syscall != kernel::TaskSyscallId::capability_call) {
            return rejected(kernel::TrapError::invalid_argument);
        }
        ++state->calls;
        state->last_id = request.arg0;
        state->last_operation = request.arg1;
        state->last_payload = request.arg2;
        return handled(request.arg0 + request.arg1 + request.arg2);
    }
};

using TableTrace = kernel::TaskSyscallTableTraceBuffer<8>;
using FrameTrace = kernel::TaskSyscallFrameTraceBuffer<32>;
using BridgeTable = kernel::TaskSyscallTable<2, TableTrace>;
using FullTable = kernel::TaskSyscallTable<4, TableTrace>;

struct SyntheticLiveFrame {
    Armv7aExceptionFrame frame{};
    Armv7aRuntimeTrapLiveFrame live{};
};

struct CallBuilderState {
    Armv7aRuntimeTrapCallPolicy policy{};
    Armv7aRuntimeTrapCallContext context{};
    SyntheticLiveFrame scratch{};
    std::uint32_t yield_builds{0};
    std::uint32_t sleep_builds{0};
    std::uint32_t debug_builds{0};
    std::uint32_t capability_builds{0};
    std::uint32_t ready_calls{0};
    std::uint64_t last_due{0};
    std::uint64_t last_debug_value{0};
    std::uint64_t last_id{0};
    std::uint64_t last_operation{0};
    std::uint64_t last_payload{0};
    std::uint64_t last_result{0};
    kernel::TrapError last_error{kernel::TrapError::none};
    bool last_writeback_seen{false};
};

[[nodiscard]] constexpr Armv7aRuntimeTrapFrameAdapterContext
make_lower_context(Armv7aRuntimeCurrentContext current) noexcept
{
    return Armv7aRuntimeTrapFrameAdapterContext{
        .policy = {},
        .ingress = armv7a_make_runtime_trap_ingress_context(current),
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

bool make_yield_frame(void* ctx,
                      kernel::TrapYieldCurrentView,
                      Armv7aRuntimeTrapLiveFrame& out) noexcept
{
    auto* state = static_cast<CallBuilderState*>(ctx);
    if (state == nullptr) {
        return false;
    }
    ++state->yield_builds;
    bind_svc_live_frame(state->scratch,
                        kArmv7aRuntimeBridgeYieldServiceId,
                        state->policy.yield_event_id,
                        state->policy.yield_event_payload,
                        0u,
                        0u,
                        state->context.origin_psr,
                        state->context.return_pc);
    out = state->scratch.live;
    return true;
}

bool make_sleep_frame(void* ctx,
                      kernel::TrapSleepUntilView<std::uint64_t> sleep,
                      Armv7aRuntimeTrapLiveFrame& out) noexcept
{
    auto* state = static_cast<CallBuilderState*>(ctx);
    if (state == nullptr) {
        return false;
    }
    ++state->sleep_builds;
    state->last_due = sleep.due;
    bind_svc_live_frame(
        state->scratch,
        kArmv7aRuntimeBridgeSleepServiceId,
        static_cast<std::uint32_t>(sleep.due & 0xFFFF'FFFFull),
        static_cast<std::uint32_t>((sleep.due >> 32u) & 0xFFFF'FFFFull),
        state->policy.sleep_event_id,
        armv7a_runtime_trap_sleep_call_payload(state->policy, sleep.due),
        state->context.origin_psr,
        state->context.return_pc + 4u);
    out = state->scratch.live;
    return true;
}

bool make_debug_frame(void* ctx,
                      kernel::TrapDebugWriteView write,
                      Armv7aRuntimeTrapLiveFrame& out) noexcept
{
    auto* state = static_cast<CallBuilderState*>(ctx);
    if (state == nullptr) {
        return false;
    }
    ++state->debug_builds;
    state->last_debug_value = write.value;
    bind_svc_live_frame(state->scratch,
                        kArmv7aRuntimeBridgeDebugWriteServiceId,
                        static_cast<std::uint32_t>(write.value),
                        0u,
                        0u,
                        0u,
                        state->context.origin_psr,
                        state->context.return_pc + 8u);
    out = state->scratch.live;
    return true;
}

bool make_capability_frame(void* ctx,
                           kernel::TrapCapabilityCallView capability,
                           Armv7aRuntimeTrapLiveFrame& out) noexcept
{
    auto* state = static_cast<CallBuilderState*>(ctx);
    if (state == nullptr) {
        return false;
    }
    ++state->capability_builds;
    state->last_id = capability.capability_id;
    state->last_operation = capability.operation;
    state->last_payload = capability.payload;
    bind_svc_live_frame(state->scratch,
                        kArmv7aRuntimeBridgeCapabilityCallServiceId,
                        static_cast<std::uint32_t>(capability.capability_id),
                        static_cast<std::uint32_t>(capability.operation),
                        static_cast<std::uint32_t>(capability.payload),
                        0u,
                        state->context.origin_psr,
                        state->context.return_pc + 12u);
    out = state->scratch.live;
    return true;
}

bool call_frame_result_ready(void* ctx,
                             const Armv7aRuntimeTrapLiveFrame& frame,
                             const kernel::TrapResult& result) noexcept
{
    auto* state = static_cast<CallBuilderState*>(ctx);
    if (state == nullptr || frame.frame == nullptr) {
        return false;
    }
    ++state->ready_calls;
    state->last_result = result.value;
    state->last_error = result.error;
    state->last_writeback_seen =
        frame.frame->r0 == static_cast<std::uint32_t>(result.value);
    return state->last_writeback_seen &&
           result.error == kernel::TrapError::none;
}

struct TaskRuntimeTransport {
    using tick_type = std::uint64_t;
    using caller_type =
        kernel::TaskSyscallFrameCaller<Armv7aRuntimeTrapLiveFrame, tick_type>;

    caller_type* caller{nullptr};

    [[nodiscard]] bool valid() const noexcept
    {
        return caller != nullptr && caller->valid();
    }

    [[nodiscard]] kernel::TrapResult yield_current(
        kernel::TrapYieldCurrentView yield) const noexcept
    {
        return caller != nullptr
            ? caller->yield(yield)
            : rejected(kernel::TrapError::unbound_bridge);
    }

    [[nodiscard]] kernel::TrapResult sleep_current_until(
        kernel::TrapSleepUntilView<tick_type> sleep) const noexcept
    {
        return caller != nullptr
            ? caller->sleep_until(sleep)
            : rejected(kernel::TrapError::unbound_bridge);
    }

    [[nodiscard]] kernel::TrapResult debug_write(
        kernel::TrapDebugWriteView write) const noexcept
    {
        return caller != nullptr
            ? caller->debug_write(write)
            : rejected(kernel::TrapError::unbound_bridge);
    }

    [[nodiscard]] kernel::TrapResult capability_call(
        kernel::TrapCapabilityCallView capability) const noexcept
    {
        return caller != nullptr
            ? caller->capability_call(capability)
            : rejected(kernel::TrapError::unbound_bridge);
    }
};

[[nodiscard]] auto make_bridge_table(DebugHandler& debug_handler,
                                     CapabilityHandler& capability_handler,
                                     TableTrace* trace) noexcept -> BridgeTable
{
    return kernel::make_task_syscall_table(
        std::array<kernel::TaskSyscallHandlerEntry, 2>{
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::debug_write,
                kernel::make_task_syscall_handler(debug_handler)),
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::capability_call,
                kernel::make_task_syscall_handler(capability_handler)),
        },
        trace);
}

[[nodiscard]] auto make_full_table(YieldHandler& yield_handler,
                                   SleepHandler& sleep_handler,
                                   DebugHandler& debug_handler,
                                   CapabilityHandler& capability_handler,
                                   TableTrace* trace) noexcept -> FullTable
{
    return kernel::make_task_syscall_table(
        std::array<kernel::TaskSyscallHandlerEntry, 4>{
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::yield,
                kernel::make_task_syscall_handler(yield_handler)),
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::sleep_until,
                kernel::make_task_syscall_handler(sleep_handler)),
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::debug_write,
                kernel::make_task_syscall_handler(debug_handler)),
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::capability_call,
                kernel::make_task_syscall_handler(capability_handler)),
        },
        trace);
}

[[nodiscard]] bool probe_generic_capture() noexcept
{
    auto lower_context = make_lower_context(Armv7aRuntimeCurrentContext{
        .stack_pointer = kStack,
        .task = kTask,
        .task_valid = true,
    });
    auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);
    Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
        .lower = lower_adapter,
    };
    auto generic_adapter = armv7a_make_kernel_runtime_trap_frame_adapter(wrapper);

    SyntheticLiveFrame debug_frame{};
    SyntheticLiveFrame capability_frame{};
    bind_svc_live_frame(debug_frame,
                        kArmv7aRuntimeBridgeDebugWriteServiceId,
                        0x33u,
                        0u,
                        0u,
                        0u,
                        kSysMode,
                        0x8100u);
    bind_svc_live_frame(capability_frame,
                        kArmv7aRuntimeBridgeCapabilityCallServiceId,
                        7u,
                        2u,
                        33u,
                        0u,
                        kUsrMode,
                        0x8104u);

    kernel::TrapFrameView debug_view{};
    kernel::TrapFrameView capability_view{};
    const auto debug_ok = generic_adapter.capture(
        generic_adapter.ctx, debug_frame.live, debug_view);
    const auto capability_ok = generic_adapter.capture(
        generic_adapter.ctx, capability_frame.live, capability_view);
    const auto apply_ok = generic_adapter.apply_result(
        generic_adapter.ctx, debug_frame.live, handled(0x55u));

    return kernel::runtime_trap_frame_adapter_ready(generic_adapter) &&
           debug_ok && capability_ok && apply_ok &&
           debug_view.service_id ==
               static_cast<std::uint16_t>(kernel::TrapService::debug_write) &&
           debug_view.task_valid &&
           debug_view.task.value == static_cast<std::size_t>(kTask) &&
           debug_view.stack_pointer == kStack &&
           capability_view.service_id == static_cast<std::uint16_t>(
               kernel::TrapService::capability_call) &&
           capability_view.arg0 == 7u && capability_view.arg1 == 2u &&
           capability_view.arg2 == 33u && debug_frame.frame.r0 == 0x55u;
}

[[nodiscard]] bool probe_ingress_adapter() noexcept
{
    auto lower_context = make_lower_context(Armv7aRuntimeCurrentContext{
        .stack_pointer = kStack,
        .task = kTask,
        .task_valid = true,
    });
    auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);
    Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
        .lower = lower_adapter,
    };
    auto generic_adapter = armv7a_make_kernel_runtime_trap_frame_adapter(wrapper);
    auto ingress_adapter =
        kernel::make_task_syscall_frame_ingress_adapter(generic_adapter);
    auto frame_adapter =
        kernel::make_task_syscall_frame_adapter(ingress_adapter);

    SyntheticLiveFrame capability_frame{};
    SyntheticLiveFrame debug_frame{};
    SyntheticLiveFrame invalid_frame{};
    bind_svc_live_frame(capability_frame,
                        kArmv7aRuntimeBridgeCapabilityCallServiceId,
                        7u,
                        2u,
                        33u,
                        0u,
                        kUsrMode,
                        0x8200u);
    bind_svc_live_frame(debug_frame,
                        kArmv7aRuntimeBridgeDebugWriteServiceId,
                        0xCCu,
                        0u,
                        0u,
                        0u,
                        kSysMode,
                        0x8204u);
    bind_svc_live_frame(invalid_frame,
                        kArmv7aRuntimeBridgeDebugWriteServiceId,
                        0xEEu,
                        0u,
                        0u,
                        0u,
                        0u,
                        0x8208u);

    kernel::TaskSyscallFrameView capability_view{};
    kernel::TaskSyscallFrameView debug_view{};
    kernel::TaskSyscallFrameView invalid_view{};
    const auto capability_ok =
        ingress_adapter.capture(capability_frame.live, capability_view);
    const auto capability_apply = ingress_adapter.apply_result(
        capability_frame.live, handled(kCapabilityResult));
    const auto debug_ok = frame_adapter.capture(
        frame_adapter.ctx, debug_frame.live, debug_view);
    const auto debug_apply = frame_adapter.apply_result(
        frame_adapter.ctx, debug_frame.live, handled(0xCCu));
    const auto invalid_ok =
        ingress_adapter.capture(invalid_frame.live, invalid_view);

    return kernel::task_syscall_frame_ingress_adapter_ready(ingress_adapter) &&
           kernel::task_syscall_frame_adapter_ready(frame_adapter) &&
           capability_ok && capability_apply && debug_ok && debug_apply &&
           !invalid_ok &&
           capability_view.syscall == kernel::TaskSyscallId::capability_call &&
           capability_frame.frame.r0 ==
               static_cast<std::uint32_t>(kCapabilityResult) &&
           debug_view.syscall == kernel::TaskSyscallId::debug_write &&
           debug_frame.frame.r0 == 0xCCu;
}

[[nodiscard]] bool probe_bridge() noexcept
{
    DebugState debug_state{};
    CapabilityState capability_state{};
    DebugHandler debug_handler{.state = &debug_state};
    CapabilityHandler capability_handler{.state = &capability_state};
    TableTrace table_trace{};
    FrameTrace frame_trace{};
    auto table = make_bridge_table(debug_handler, capability_handler, &table_trace);

    auto lower_context = make_lower_context(Armv7aRuntimeCurrentContext{
        .stack_pointer = kStack,
        .task = kTask,
        .task_valid = true,
    });
    auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);
    Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
        .lower = lower_adapter,
    };
    auto bridge = armv7a_make_task_syscall_frame_bridge(
        table, wrapper, &frame_trace);

    SyntheticLiveFrame debug_frame{};
    SyntheticLiveFrame capability_frame{};
    bind_svc_live_frame(debug_frame,
                        kArmv7aRuntimeBridgeDebugWriteServiceId,
                        static_cast<std::uint32_t>(kDebugValue),
                        0u,
                        0u,
                        0u,
                        kSysMode,
                        0x8300u);
    bind_svc_live_frame(capability_frame,
                        kArmv7aRuntimeBridgeCapabilityCallServiceId,
                        static_cast<std::uint32_t>(kCapabilityId),
                        static_cast<std::uint32_t>(kCapabilityOperation),
                        static_cast<std::uint32_t>(kCapabilityPayload),
                        0u,
                        kUsrMode,
                        0x8304u);

    const auto debug_result = bridge.dispatch(debug_frame.live);
    const auto capability_result = bridge.dispatch(capability_frame.live);
    const auto* debug_decode = frame_trace.at(0u);
    const auto* debug_writeback = frame_trace.at(2u);
    const auto* capability_decode = frame_trace.at(3u);
    const auto* capability_writeback = frame_trace.at(5u);
    const auto* table_first = table_trace.at(0u);
    const auto* table_second = table_trace.at(1u);

    return bridge.valid() &&
           result_matches(debug_result,
                          kernel::TrapDisposition::handled,
                          kernel::TrapError::none,
                          kDebugValue + 1u) &&
           result_matches(capability_result,
                          kernel::TrapDisposition::handled,
                          kernel::TrapError::none,
                          kCapabilityResult) &&
           debug_state.calls == 1u && debug_state.last_value == kDebugValue &&
           capability_state.calls == 1u &&
           capability_state.last_id == kCapabilityId &&
           capability_state.last_operation == kCapabilityOperation &&
           capability_state.last_payload == kCapabilityPayload &&
           debug_frame.frame.r0 == static_cast<std::uint32_t>(kDebugValue + 1u) &&
           capability_frame.frame.r0 ==
               static_cast<std::uint32_t>(kCapabilityResult) &&
           debug_decode != nullptr && debug_writeback != nullptr &&
           capability_decode != nullptr && capability_writeback != nullptr &&
           table_first != nullptr && table_second != nullptr &&
           debug_decode->syscall == kernel::TaskSyscallId::debug_write &&
           debug_writeback->value == kDebugValue + 1u &&
           capability_decode->syscall ==
               kernel::TaskSyscallId::capability_call &&
           capability_writeback->value == kCapabilityResult &&
           table_first->syscall == kernel::TaskSyscallId::debug_write &&
           table_second->syscall == kernel::TaskSyscallId::capability_call;
}

struct CallerSummary {
    std::uint64_t yield_result{0u};
    std::uint64_t sleep_result{0u};
    std::uint64_t debug_result{0u};
    std::uint64_t capability_result{0u};
    bool caller_ready{false};
    bool runtime_api_ready{false};
};

[[nodiscard]] CallerSummary probe_caller() noexcept
{
    YieldState yield_state{};
    SleepState sleep_state{};
    DebugState debug_state{};
    CapabilityState capability_state{};
    YieldHandler yield_handler{.state = &yield_state};
    SleepHandler sleep_handler{.state = &sleep_state};
    DebugHandler debug_handler{.state = &debug_state};
    CapabilityHandler capability_handler{.state = &capability_state};
    TableTrace table_trace{};
    FrameTrace frame_trace{};
    auto table = make_full_table(yield_handler,
                                 sleep_handler,
                                 debug_handler,
                                 capability_handler,
                                 &table_trace);

    auto lower_context = make_lower_context(Armv7aRuntimeCurrentContext{
        .stack_pointer = kStack,
        .task = kTask,
        .task_valid = true,
    });
    auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(lower_context);
    Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
        .lower = lower_adapter,
    };
    auto frame_bridge = armv7a_make_task_syscall_frame_bridge(
        table, wrapper, &frame_trace);
    auto port = frame_bridge.port();

    CallBuilderState builder_state{
        .policy =
            Armv7aRuntimeTrapCallPolicy{
                .yield_event_id = 0u,
                .yield_event_payload = 0u,
                .sleep_event_id = 0u,
                .sleep_event_payload = 0u,
                .sleep_payload_matches_due_low32 = true,
            },
        .context =
            Armv7aRuntimeTrapCallContext{
                .origin_psr = kUsrMode,
                .handler_psr = kSvcMode,
                .return_pc = 0x8400u,
                .stack_pointer = kStack,
                .task = kTask,
                .task_valid = true,
            },
    };
    auto runtime_call_adapter =
        kernel::RuntimeTrapCallFrameAdapter<Armv7aRuntimeTrapLiveFrame,
                                            std::uint64_t>{
            .ctx = &builder_state,
            .make_yield_frame = &make_yield_frame,
            .make_sleep_frame = &make_sleep_frame,
            .make_debug_write_frame = &make_debug_frame,
            .make_capability_call_frame = &make_capability_frame,
            .result_ready = &call_frame_result_ready,
        };
    auto caller =
        armv7a_make_task_syscall_frame_caller(port, runtime_call_adapter);

    const auto yielded = caller.yield();
    const auto slept = caller.sleep_until(kSleepDue);
    const auto debugged = caller.debug_write(kDebugValue);
    const auto called =
        caller.capability_call(kCapabilityId,
                               kCapabilityOperation,
                               kCapabilityPayload);

    const auto* first = frame_trace.at(0u);
    const auto* fourth = frame_trace.at(3u);
    const auto* seventh = frame_trace.at(6u);
    const auto* tenth = frame_trace.at(9u);
    const auto* table_first = table_trace.at(0u);
    const auto* table_fourth = table_trace.at(3u);

    const bool caller_ready =
        frame_bridge.valid() && port.valid() && caller.valid() &&
        result_matches(yielded,
                       kernel::TrapDisposition::handled,
                       kernel::TrapError::none,
                       1u) &&
        result_matches(slept,
                       kernel::TrapDisposition::handled,
                       kernel::TrapError::none,
                       kSleepDue) &&
        result_matches(debugged,
                       kernel::TrapDisposition::handled,
                       kernel::TrapError::none,
                       kDebugValue + 1u) &&
        result_matches(called,
                       kernel::TrapDisposition::handled,
                       kernel::TrapError::none,
                       kCapabilityResult) &&
        builder_state.yield_builds == 1u &&
        builder_state.sleep_builds == 1u &&
        builder_state.debug_builds == 1u &&
        builder_state.capability_builds == 1u &&
        builder_state.ready_calls == 4u &&
        builder_state.last_due == kSleepDue &&
        builder_state.last_debug_value == kDebugValue &&
        builder_state.last_id == kCapabilityId &&
        builder_state.last_operation == kCapabilityOperation &&
        builder_state.last_payload == kCapabilityPayload &&
        builder_state.last_result == kCapabilityResult &&
        builder_state.last_error == kernel::TrapError::none &&
        builder_state.last_writeback_seen && yield_state.calls == 1u &&
        sleep_state.calls == 1u && sleep_state.last_due == kSleepDue &&
        debug_state.calls == 1u && debug_state.last_value == kDebugValue &&
        capability_state.calls == 1u &&
        capability_state.last_id == kCapabilityId &&
        capability_state.last_operation == kCapabilityOperation &&
        capability_state.last_payload == kCapabilityPayload &&
        first != nullptr && fourth != nullptr && seventh != nullptr &&
        tenth != nullptr && table_first != nullptr &&
        table_fourth != nullptr &&
        first->syscall == kernel::TaskSyscallId::yield &&
        fourth->syscall == kernel::TaskSyscallId::sleep_until &&
        seventh->syscall == kernel::TaskSyscallId::debug_write &&
        tenth->syscall == kernel::TaskSyscallId::capability_call &&
        table_first->syscall == kernel::TaskSyscallId::yield &&
        table_fourth->syscall == kernel::TaskSyscallId::capability_call;

    const auto runtime_services = kernel::make_runtime_trap_service_facade(
        TaskRuntimeTransport{
            .caller = &caller,
        });
    const auto runtime_api = kernel::make_task_runtime_api(runtime_services);
    const auto api_yield = runtime_api.yield();
    const auto api_sleep = runtime_api.sleep_until(kRuntimeApiSleepDue);
    const auto api_debug = runtime_api.debug_write(kRuntimeApiDebugValue);
    const auto api_capability = runtime_api.capability_call(
        kRuntimeApiCapabilityId,
        kRuntimeApiCapabilityOperation,
        kRuntimeApiCapabilityPayload);

    const auto* thirteenth = frame_trace.at(12u);
    const auto* sixteenth = frame_trace.at(15u);
    const auto* nineteenth = frame_trace.at(18u);
    const auto* twenty_second = frame_trace.at(21u);
    const auto* table_fifth = table_trace.at(4u);
    const auto* table_eighth = table_trace.at(7u);

    return CallerSummary{
        .yield_result = yielded.value,
        .sleep_result = slept.value,
        .debug_result = debugged.value,
        .capability_result = called.value,
        .caller_ready = caller_ready,
        .runtime_api_ready =
            caller_ready && runtime_services.valid() && runtime_api.valid() &&
            result_matches(api_yield,
                           kernel::TrapDisposition::handled,
                           kernel::TrapError::none,
                           1u) &&
            result_matches(api_sleep,
                           kernel::TrapDisposition::handled,
                           kernel::TrapError::none,
                           kRuntimeApiSleepDue) &&
            result_matches(api_debug,
                           kernel::TrapDisposition::handled,
                           kernel::TrapError::none,
                           kRuntimeApiDebugValue + 1u) &&
            result_matches(api_capability,
                           kernel::TrapDisposition::handled,
                           kernel::TrapError::none,
                           kRuntimeApiCapabilityResult) &&
            builder_state.yield_builds == 2u &&
            builder_state.sleep_builds == 2u &&
            builder_state.debug_builds == 2u &&
            builder_state.capability_builds == 2u &&
            builder_state.ready_calls == 8u &&
            builder_state.last_due == kRuntimeApiSleepDue &&
            builder_state.last_debug_value == kRuntimeApiDebugValue &&
            builder_state.last_id == kRuntimeApiCapabilityId &&
            builder_state.last_operation == kRuntimeApiCapabilityOperation &&
            builder_state.last_payload == kRuntimeApiCapabilityPayload &&
            builder_state.last_result == kRuntimeApiCapabilityResult &&
            builder_state.last_error == kernel::TrapError::none &&
            builder_state.last_writeback_seen && yield_state.calls == 2u &&
            sleep_state.calls == 2u &&
            sleep_state.last_due == kRuntimeApiSleepDue &&
            debug_state.calls == 2u &&
            debug_state.last_value == kRuntimeApiDebugValue &&
            capability_state.calls == 2u &&
            capability_state.last_id == kRuntimeApiCapabilityId &&
            capability_state.last_operation ==
                kRuntimeApiCapabilityOperation &&
            capability_state.last_payload == kRuntimeApiCapabilityPayload &&
            thirteenth != nullptr && sixteenth != nullptr &&
            nineteenth != nullptr && twenty_second != nullptr &&
            table_fifth != nullptr && table_eighth != nullptr &&
            thirteenth->syscall == kernel::TaskSyscallId::yield &&
            sixteenth->syscall == kernel::TaskSyscallId::sleep_until &&
            nineteenth->syscall == kernel::TaskSyscallId::debug_write &&
            twenty_second->syscall == kernel::TaskSyscallId::capability_call &&
            table_fifth->syscall == kernel::TaskSyscallId::yield &&
            table_eighth->syscall ==
                kernel::TaskSyscallId::capability_call,
    };
}
} // namespace

Armv7aTaskSyscallGlueObservation
armv7a_capture_task_syscall_glue_observation() noexcept
{
    const auto caller = probe_caller();

    return Armv7aTaskSyscallGlueObservation{
        .task = kTask,
        .stack_pointer = kStack,
        .yield_result = caller.yield_result,
        .sleep_result = caller.sleep_result,
        .debug_result = caller.debug_result,
        .capability_result = caller.capability_result,
        .generic_ready = probe_generic_capture(),
        .ingress_ready = probe_ingress_adapter(),
        .bridge_ready = probe_bridge(),
        .caller_ready = caller.caller_ready,
        .runtime_api_ready = caller.runtime_api_ready,
    };
}

void armv7a_print_task_syscall_glue_observation()
{
    const auto observation = armv7a_capture_task_syscall_glue_observation();

    armv7a_platform_early_console_puts("ARMv7-A task syscall glue, task=0x");
    armv7a_diag_put_hex64(observation.task, 16);
    armv7a_platform_early_console_puts(", stack=0x");
    armv7a_diag_put_hex64(observation.stack_pointer, 16);
    armv7a_platform_early_console_puts(", yield=0x");
    armv7a_diag_put_hex(static_cast<std::uintptr_t>(observation.yield_result));
    armv7a_platform_early_console_puts(", sleep=0x");
    armv7a_diag_put_hex(static_cast<std::uintptr_t>(observation.sleep_result));
    armv7a_platform_early_console_puts(", debug=0x");
    armv7a_diag_put_hex(static_cast<std::uintptr_t>(observation.debug_result));
    armv7a_platform_early_console_puts(", capability=0x");
    armv7a_diag_put_hex(
        static_cast<std::uintptr_t>(observation.capability_result));
    armv7a_platform_early_console_puts(", generic=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.generic_ready));
    armv7a_platform_early_console_puts(", ingress=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.ingress_ready));
    armv7a_platform_early_console_puts(", bridge=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.bridge_ready));
    armv7a_platform_early_console_puts(", caller=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.caller_ready));
    armv7a_platform_early_console_puts(", api=");
    armv7a_platform_early_console_puts(
        armv7a_diag_yes_no(observation.runtime_api_ready));
    armv7a_platform_early_console_puts(", glue=");
    armv7a_platform_early_console_puts(armv7a_diag_yes_no(
        armv7a_task_syscall_glue_ready(observation)));
    armv7a_platform_early_console_puts("\r\n");
}

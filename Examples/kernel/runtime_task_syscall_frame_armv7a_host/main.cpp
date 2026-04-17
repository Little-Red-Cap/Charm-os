#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "targets/armv7a/common/armv7a_exception_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_bridge_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_caller_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_current_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_frame_adapter_contract.hpp"

import kernel.task_syscall_frame;
import target.armv7a.kernel_runtime_trap_frame_adapter;
import target.armv7a.kernel_task_syscall_call_frame_adapter;

namespace demo {
    inline constexpr std::uint32_t kUsrMode = 0x10u;
    inline constexpr std::uint32_t kSvcMode = 0x13u;
    inline constexpr std::uint32_t kSysMode = 0x1fu;
    inline constexpr std::uint64_t kTaskValue = 0x24u;
    inline constexpr std::uint64_t kStackPointer = 0x9000u;

    struct DebugHandlerState {
        std::uint32_t calls{0};
        std::uint64_t last_value{0};
    };

    struct YieldHandlerState {
        std::uint32_t calls{0};
    };

    struct SleepHandlerState {
        std::uint32_t calls{0};
        std::uint64_t last_due{0};
    };

    struct CapabilityHandlerState {
        std::uint32_t calls{0};
        std::uint64_t last_capability_id{0};
        std::uint64_t last_operation{0};
        std::uint64_t last_payload{0};
    };

    struct DebugHandler {
        DebugHandlerState* state{nullptr};

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

    private:
        [[nodiscard]] static constexpr kernel::TrapResult handled(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }

        [[nodiscard]] static constexpr kernel::TrapResult rejected(
            kernel::TrapError error) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = error,
                .value = 0,
            };
        }
    };

    struct YieldHandler {
        YieldHandlerState* state{nullptr};

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

    private:
        [[nodiscard]] static constexpr kernel::TrapResult handled(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }

        [[nodiscard]] static constexpr kernel::TrapResult rejected(
            kernel::TrapError error) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = error,
                .value = 0,
            };
        }
    };

    struct SleepHandler {
        SleepHandlerState* state{nullptr};

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

    private:
        [[nodiscard]] static constexpr kernel::TrapResult handled(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }

        [[nodiscard]] static constexpr kernel::TrapResult rejected(
            kernel::TrapError error) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = error,
                .value = 0,
            };
        }
    };

    struct CapabilityHandler {
        CapabilityHandlerState* state{nullptr};

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
            state->last_capability_id = request.arg0;
            state->last_operation = request.arg1;
            state->last_payload = request.arg2;
            return handled(request.arg0 + request.arg1 + request.arg2);
        }

    private:
        [[nodiscard]] static constexpr kernel::TrapResult handled(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }

        [[nodiscard]] static constexpr kernel::TrapResult rejected(
            kernel::TrapError error) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = error,
                .value = 0,
            };
        }
    };

    using TableTrace = kernel::TaskSyscallTableTraceBuffer<8>;
    using FrameTrace = kernel::TaskSyscallFrameTraceBuffer<16>;
    using StaticTable = kernel::TaskSyscallTable<2, TableTrace>;
    using FullStaticTable = kernel::TaskSyscallTable<4, TableTrace>;

    struct SyntheticArmv7aLiveFrame {
        Armv7aExceptionFrame frame{};
        Armv7aRuntimeTrapLiveFrame live{};
    };

    struct Armv7aCallFrameBuilderState {
        Armv7aRuntimeTrapCallPolicy policy{};
        Armv7aRuntimeTrapCallContext context{};
        SyntheticArmv7aLiveFrame scratch{};
        std::uint32_t yield_builds{0};
        std::uint32_t sleep_builds{0};
        std::uint32_t debug_builds{0};
        std::uint32_t capability_builds{0};
        std::uint32_t ready_calls{0};
        std::uint64_t last_due{0};
        std::uint64_t last_debug_value{0};
        std::uint64_t last_capability_id{0};
        std::uint64_t last_capability_operation{0};
        std::uint64_t last_capability_payload{0};
        std::uint64_t last_result_value{0};
        kernel::TrapError last_error{kernel::TrapError::none};
        bool last_frame_writeback_seen{false};
    };

    [[nodiscard]] constexpr kernel::TrapResult handled_result(
        std::uint64_t value) noexcept
    {
        return kernel::TrapResult{
            .disposition = kernel::TrapDisposition::handled,
            .error = kernel::TrapError::none,
            .value = value,
        };
    }

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    [[nodiscard]] constexpr Armv7aRuntimeTrapFrameAdapterContext
    make_lower_frame_adapter_context(
        Armv7aRuntimeCurrentContext current) noexcept
    {
        return Armv7aRuntimeTrapFrameAdapterContext{
            .policy = {},
            .ingress = armv7a_make_runtime_trap_ingress_context(current),
        };
    }

    void bind_svc_live_frame(SyntheticArmv7aLiveFrame& out,
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

    bool make_armv7a_yield_call_frame(
        void* ctx,
        kernel::TrapYieldCurrentView,
        Armv7aRuntimeTrapLiveFrame& out) noexcept
    {
        auto* state = static_cast<Armv7aCallFrameBuilderState*>(ctx);
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

    bool make_armv7a_sleep_call_frame(
        void* ctx,
        kernel::TrapSleepUntilView<std::uint64_t> sleep,
        Armv7aRuntimeTrapLiveFrame& out) noexcept
    {
        auto* state = static_cast<Armv7aCallFrameBuilderState*>(ctx);
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

    bool make_armv7a_debug_call_frame(
        void* ctx,
        kernel::TrapDebugWriteView write,
        Armv7aRuntimeTrapLiveFrame& out) noexcept
    {
        auto* state = static_cast<Armv7aCallFrameBuilderState*>(ctx);
        if (state == nullptr) {
            return false;
        }

        ++state->debug_builds;
        state->last_debug_value = write.value;
        bind_svc_live_frame(state->scratch,
                            kArmv7aRuntimeBridgeDebugWriteServiceId,
                            static_cast<std::uint32_t>(write.value &
                                                       0xFFFF'FFFFull),
                            0u,
                            0u,
                            0u,
                            state->context.origin_psr,
                            state->context.return_pc + 8u);
        out = state->scratch.live;
        return true;
    }

    bool make_armv7a_capability_call_frame(
        void* ctx,
        kernel::TrapCapabilityCallView capability,
        Armv7aRuntimeTrapLiveFrame& out) noexcept
    {
        auto* state = static_cast<Armv7aCallFrameBuilderState*>(ctx);
        if (state == nullptr) {
            return false;
        }

        ++state->capability_builds;
        state->last_capability_id = capability.capability_id;
        state->last_capability_operation = capability.operation;
        state->last_capability_payload = capability.payload;
        bind_svc_live_frame(state->scratch,
                            kArmv7aRuntimeBridgeCapabilityCallServiceId,
                            static_cast<std::uint32_t>(capability.capability_id &
                                                       0xFFFF'FFFFull),
                            static_cast<std::uint32_t>(capability.operation &
                                                       0xFFFF'FFFFull),
                            static_cast<std::uint32_t>(capability.payload &
                                                       0xFFFF'FFFFull),
                            0u,
                            state->context.origin_psr,
                            state->context.return_pc + 12u);
        out = state->scratch.live;
        return true;
    }

    bool armv7a_call_frame_result_ready(
        void* ctx,
        const Armv7aRuntimeTrapLiveFrame& frame,
        const kernel::TrapResult& result) noexcept
    {
        auto* state = static_cast<Armv7aCallFrameBuilderState*>(ctx);
        if (state == nullptr || frame.frame == nullptr) {
            return false;
        }

        ++state->ready_calls;
        state->last_result_value = result.value;
        state->last_error = result.error;
        state->last_frame_writeback_seen =
            frame.frame->r0 == static_cast<std::uint32_t>(result.value);
        return state->last_frame_writeback_seen &&
               result.error == kernel::TrapError::none;
    }

    [[nodiscard]] auto make_task_syscall_table(
        DebugHandler& debug_handler,
        CapabilityHandler& capability_handler,
        TableTrace* trace) noexcept -> StaticTable
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

    [[nodiscard]] auto make_full_task_syscall_table(
        YieldHandler& yield_handler,
        SleepHandler& sleep_handler,
        DebugHandler& debug_handler,
        CapabilityHandler& capability_handler,
        TableTrace* trace) noexcept -> FullStaticTable
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

    [[nodiscard]] bool probe_generic_trap_adapter_capture() noexcept
    {
        auto lower_context = make_lower_frame_adapter_context(
            Armv7aRuntimeCurrentContext{
                .stack_pointer = kStackPointer,
                .task = kTaskValue,
                .task_valid = true,
            });
        auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(
            lower_context);
        Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
            .lower = lower_adapter,
        };
        auto generic_adapter =
            armv7a_make_kernel_runtime_trap_frame_adapter(wrapper);

        SyntheticArmv7aLiveFrame debug_frame{};
        SyntheticArmv7aLiveFrame capability_frame{};
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
        const auto debug_captured = generic_adapter.capture(
            generic_adapter.ctx, debug_frame.live, debug_view);
        const auto capability_captured = generic_adapter.capture(
            generic_adapter.ctx, capability_frame.live, capability_view);
        const auto applied = generic_adapter.apply_result(
            generic_adapter.ctx, debug_frame.live, handled_result(0x55u));

        return kernel::runtime_trap_frame_adapter_ready(generic_adapter) &&
               debug_captured && capability_captured && applied &&
               debug_view.service_id == static_cast<std::uint16_t>(
                   kernel::TrapService::debug_write) &&
               debug_view.arg0 == 0x33u &&
               debug_view.return_pc == 0x8100u &&
               debug_view.stack_pointer == kStackPointer &&
               debug_view.status == kSysMode &&
               debug_view.origin == kernel::TrapOrigin::kernel_thread &&
               debug_view.task.value ==
                   static_cast<std::size_t>(kTaskValue) &&
               debug_view.task_valid &&
               capability_view.service_id == static_cast<std::uint16_t>(
                   kernel::TrapService::capability_call) &&
               capability_view.arg0 == 7u && capability_view.arg1 == 2u &&
               capability_view.arg2 == 33u &&
               capability_view.return_pc == 0x8104u &&
               capability_view.origin == kernel::TrapOrigin::user_task &&
               capability_view.task_valid &&
               debug_frame.frame.r0 == 0x55u &&
               debug_frame.frame.r1 == 0u &&
               debug_frame.frame.lr == 0x8100u;
    }

    [[nodiscard]] bool probe_task_syscall_ingress_adapter() noexcept
    {
        auto lower_context = make_lower_frame_adapter_context(
            Armv7aRuntimeCurrentContext{
                .stack_pointer = kStackPointer,
                .task = kTaskValue,
                .task_valid = true,
            });
        auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(
            lower_context);
        Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
            .lower = lower_adapter,
        };
        auto generic_adapter =
            armv7a_make_kernel_runtime_trap_frame_adapter(wrapper);
        auto ingress_adapter =
            kernel::make_task_syscall_frame_ingress_adapter(generic_adapter);
        auto frame_adapter =
            kernel::make_task_syscall_frame_adapter(ingress_adapter);

        SyntheticArmv7aLiveFrame capability_frame{};
        SyntheticArmv7aLiveFrame debug_frame{};
        SyntheticArmv7aLiveFrame invalid_frame{};
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
        const auto captured = ingress_adapter.capture(
            capability_frame.live, capability_view);
        const auto applied = ingress_adapter.apply_result(
            capability_frame.live, handled_result(42u));
        const auto bridged_captured = frame_adapter.capture(
            frame_adapter.ctx, debug_frame.live, debug_view);
        const auto bridged_applied = frame_adapter.apply_result(
            frame_adapter.ctx, debug_frame.live, handled_result(0xCCu));
        const auto invalid_captured = ingress_adapter.capture(
            invalid_frame.live, invalid_view);

        return kernel::task_syscall_frame_ingress_adapter_ready(
                   ingress_adapter) &&
               kernel::task_syscall_frame_adapter_ready(frame_adapter) &&
               captured && applied && bridged_captured && bridged_applied &&
               !invalid_captured &&
               capability_view.syscall ==
                   kernel::TaskSyscallId::capability_call &&
               capability_view.arg0 == 7u && capability_view.arg1 == 2u &&
               capability_view.arg2 == 33u &&
               capability_frame.frame.r0 == 42u &&
               debug_view.syscall == kernel::TaskSyscallId::debug_write &&
               debug_view.arg0 == 0xCCu &&
               debug_frame.frame.r0 == 0xCCu;
    }

    [[nodiscard]] bool probe_task_syscall_bridge() noexcept
    {
        DebugHandlerState debug_state{};
        CapabilityHandlerState capability_state{};
        DebugHandler debug_handler{
            .state = &debug_state,
        };
        CapabilityHandler capability_handler{
            .state = &capability_state,
        };
        TableTrace table_trace{};
        auto table =
            make_task_syscall_table(debug_handler, capability_handler, &table_trace);

        auto lower_context = make_lower_frame_adapter_context(
            Armv7aRuntimeCurrentContext{
                .stack_pointer = kStackPointer,
                .task = kTaskValue,
                .task_valid = true,
            });
        auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(
            lower_context);
        Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
            .lower = lower_adapter,
        };
        auto generic_adapter =
            armv7a_make_kernel_runtime_trap_frame_adapter(wrapper);

        FrameTrace direct_trace{};
        auto direct_bridge = kernel::make_task_syscall_frame_bridge(
            table, generic_adapter, &direct_trace);

        auto ingress_adapter =
            kernel::make_task_syscall_frame_ingress_adapter(generic_adapter);
        FrameTrace ingress_trace{};
        auto ingress_bridge = kernel::make_task_syscall_frame_bridge(
            table, ingress_adapter, &ingress_trace);

        FrameTrace invalid_trace{};
        auto invalid_bridge = kernel::make_task_syscall_frame_bridge(
            table, ingress_adapter, &invalid_trace);

        SyntheticArmv7aLiveFrame debug_frame{};
        SyntheticArmv7aLiveFrame capability_frame{};
        SyntheticArmv7aLiveFrame invalid_frame{};
        bind_svc_live_frame(debug_frame,
                            kArmv7aRuntimeBridgeDebugWriteServiceId,
                            0x21u,
                            0u,
                            0u,
                            0u,
                            kSysMode,
                            0x8300u);
        bind_svc_live_frame(capability_frame,
                            kArmv7aRuntimeBridgeCapabilityCallServiceId,
                            7u,
                            3u,
                            5u,
                            0u,
                            kUsrMode,
                            0x8304u);
        bind_svc_live_frame(invalid_frame,
                            kArmv7aRuntimeBridgeDebugWriteServiceId,
                            0x44u,
                            0u,
                            0u,
                            0u,
                            0u,
                            0x8308u);

        const auto direct_result = direct_bridge.dispatch(debug_frame.live);
        const auto ingress_result = ingress_bridge.dispatch(capability_frame.live);
        const auto invalid_result = invalid_bridge.dispatch(invalid_frame.live);

        const auto* direct_decode = direct_trace.at(0u);
        const auto* direct_dispatch = direct_trace.at(1u);
        const auto* direct_writeback = direct_trace.at(2u);
        const auto* ingress_decode = ingress_trace.at(0u);
        const auto* ingress_dispatch = ingress_trace.at(1u);
        const auto* ingress_writeback = ingress_trace.at(2u);
        const auto* invalid_decode = invalid_trace.at(0u);
        const auto* table_first = table_trace.at(0u);
        const auto* table_second = table_trace.at(1u);

        if (direct_decode == nullptr || direct_dispatch == nullptr ||
            direct_writeback == nullptr || ingress_decode == nullptr ||
            ingress_dispatch == nullptr || ingress_writeback == nullptr ||
            invalid_decode == nullptr || table_first == nullptr ||
            table_second == nullptr) {
            return false;
        }

        return trap_result_matches(direct_result,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0x22u) &&
               trap_result_matches(ingress_result,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   15u) &&
               trap_result_matches(invalid_result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::decode_failed,
                                   0u) &&
               debug_state.calls == 1u && debug_state.last_value == 0x21u &&
               capability_state.calls == 1u &&
               capability_state.last_capability_id == 7u &&
               capability_state.last_operation == 3u &&
               capability_state.last_payload == 5u &&
               debug_frame.frame.r0 == 0x22u &&
               capability_frame.frame.r0 == 15u &&
               invalid_frame.frame.r0 == 0x44u &&
               direct_decode->stage == kernel::TaskSyscallFrameStage::decode &&
               direct_decode->syscall == kernel::TaskSyscallId::debug_write &&
               direct_decode->ok &&
               direct_dispatch->stage ==
                   kernel::TaskSyscallFrameStage::dispatch &&
               direct_dispatch->value == 0x22u && direct_dispatch->ok &&
               direct_writeback->stage ==
                   kernel::TaskSyscallFrameStage::writeback &&
               direct_writeback->value == 0x22u && direct_writeback->ok &&
               ingress_decode->stage ==
                   kernel::TaskSyscallFrameStage::decode &&
               ingress_decode->syscall ==
                   kernel::TaskSyscallId::capability_call &&
               ingress_decode->ok &&
               ingress_dispatch->stage ==
                   kernel::TaskSyscallFrameStage::dispatch &&
               ingress_dispatch->value == 15u && ingress_dispatch->ok &&
               ingress_writeback->stage ==
                   kernel::TaskSyscallFrameStage::writeback &&
               ingress_writeback->value == 15u && ingress_writeback->ok &&
               invalid_decode->stage == kernel::TaskSyscallFrameStage::decode &&
               invalid_decode->error == kernel::TrapError::decode_failed &&
               !invalid_decode->ok && table_trace.size() == 2u &&
               table_first->syscall == kernel::TaskSyscallId::debug_write &&
               table_first->value == 0x22u && table_first->matched &&
               table_second->syscall ==
                   kernel::TaskSyscallId::capability_call &&
               table_second->value == 15u && table_second->matched;
    }

    [[nodiscard]] bool probe_task_syscall_caller_adapter() noexcept
    {
        YieldHandlerState yield_state{};
        SleepHandlerState sleep_state{};
        DebugHandlerState debug_state{};
        CapabilityHandlerState capability_state{};
        YieldHandler yield_handler{
            .state = &yield_state,
        };
        SleepHandler sleep_handler{
            .state = &sleep_state,
        };
        DebugHandler debug_handler{
            .state = &debug_state,
        };
        CapabilityHandler capability_handler{
            .state = &capability_state,
        };
        TableTrace table_trace{};
        FrameTrace frame_trace{};
        auto table = make_full_task_syscall_table(yield_handler,
                                                  sleep_handler,
                                                  debug_handler,
                                                  capability_handler,
                                                  &table_trace);

        auto lower_context = make_lower_frame_adapter_context(
            Armv7aRuntimeCurrentContext{
                .stack_pointer = kStackPointer,
                .task = kTaskValue,
                .task_valid = true,
            });
        auto lower_adapter = armv7a_make_runtime_trap_frame_adapter(
            lower_context);
        Armv7aKernelRuntimeTrapFrameAdapterContext wrapper{
            .lower = lower_adapter,
        };
        auto frame_bridge = armv7a_make_task_syscall_frame_bridge(
            table, wrapper, &frame_trace);
        auto port = frame_bridge.port();

        Armv7aCallFrameBuilderState builder_state{
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
                    .stack_pointer = kStackPointer,
                    .task = kTaskValue,
                    .task_valid = true,
                },
        };
        auto runtime_call_adapter =
            kernel::RuntimeTrapCallFrameAdapter<Armv7aRuntimeTrapLiveFrame,
                                                std::uint64_t>{
                .ctx = &builder_state,
                .make_yield_frame = &make_armv7a_yield_call_frame,
                .make_sleep_frame = &make_armv7a_sleep_call_frame,
                .make_debug_write_frame = &make_armv7a_debug_call_frame,
                .make_capability_call_frame =
                    &make_armv7a_capability_call_frame,
                .result_ready = &armv7a_call_frame_result_ready,
            };
        auto caller = armv7a_make_task_syscall_frame_caller(
            port, runtime_call_adapter);

        const auto yielded = caller.yield();
        const auto slept = caller.sleep_until(55u);
        const auto debugged = caller.debug_write(0xCCu);
        const auto called = caller.capability_call(7u, 2u, 33u);

        const auto* first = frame_trace.at(0u);
        const auto* fourth = frame_trace.at(3u);
        const auto* seventh = frame_trace.at(6u);
        const auto* tenth = frame_trace.at(9u);
        const auto* table_first = table_trace.at(0u);
        const auto* table_second = table_trace.at(1u);
        const auto* table_third = table_trace.at(2u);
        const auto* table_fourth = table_trace.at(3u);
        if (first == nullptr || fourth == nullptr || seventh == nullptr ||
            tenth == nullptr || table_first == nullptr ||
            table_second == nullptr || table_third == nullptr ||
            table_fourth == nullptr) {
            return false;
        }

        return trap_result_matches(yielded,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               trap_result_matches(slept,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   55u) &&
               trap_result_matches(debugged,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0xCDu) &&
               trap_result_matches(called,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               builder_state.yield_builds == 1u &&
               builder_state.sleep_builds == 1u &&
               builder_state.debug_builds == 1u &&
               builder_state.capability_builds == 1u &&
               builder_state.ready_calls == 4u &&
               builder_state.last_due == 55u &&
               builder_state.last_debug_value == 0xCCu &&
               builder_state.last_capability_id == 7u &&
               builder_state.last_capability_operation == 2u &&
               builder_state.last_capability_payload == 33u &&
               builder_state.last_result_value == 42u &&
               builder_state.last_error == kernel::TrapError::none &&
               builder_state.last_frame_writeback_seen &&
               yield_state.calls == 1u && sleep_state.calls == 1u &&
               sleep_state.last_due == 55u && debug_state.calls == 1u &&
               debug_state.last_value == 0xCCu &&
               capability_state.calls == 1u &&
               capability_state.last_capability_id == 7u &&
               capability_state.last_operation == 2u &&
               capability_state.last_payload == 33u &&
               first->syscall == kernel::TaskSyscallId::yield &&
               fourth->syscall == kernel::TaskSyscallId::sleep_until &&
               seventh->syscall == kernel::TaskSyscallId::debug_write &&
               tenth->syscall == kernel::TaskSyscallId::capability_call &&
               table_first->syscall == kernel::TaskSyscallId::yield &&
               table_first->value == 1u &&
               table_second->syscall == kernel::TaskSyscallId::sleep_until &&
               table_second->value == 55u &&
               table_third->syscall == kernel::TaskSyscallId::debug_write &&
               table_third->value == 0xCDu &&
               table_fourth->syscall ==
                   kernel::TaskSyscallId::capability_call &&
               table_fourth->value == 42u;
    }
} // namespace demo

int main()
{
    const bool generic = demo::probe_generic_trap_adapter_capture();
    const bool ingress = demo::probe_task_syscall_ingress_adapter();
    const bool bridge = demo::probe_task_syscall_bridge();
    const bool caller = demo::probe_task_syscall_caller_adapter();
    const bool ok = generic && ingress && bridge && caller;

    std::printf("ok=%d generic=%d ingress=%d bridge=%d caller=%d\n",
                ok ? 1 : 0,
                generic ? 1 : 0,
                ingress ? 1 : 0,
                bridge ? 1 : 0,
                caller ? 1 : 0);
    return ok ? 0 : 1;
}

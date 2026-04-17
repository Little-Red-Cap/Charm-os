#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "targets/armv7a/common/armv7a_exception_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_bridge_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_current_contract.hpp"
#include "targets/armv7a/common/armv7a_runtime_trap_frame_adapter_contract.hpp"

import kernel.task_syscall_frame;

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

    struct Armv7aKernelTrapAdapterContext {
        Armv7aRuntimeTrapFrameAdapter lower{};
    };

    struct SyntheticArmv7aLiveFrame {
        Armv7aExceptionFrame frame{};
        Armv7aRuntimeTrapLiveFrame live{};
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

    [[nodiscard]] constexpr bool map_armv7a_origin(
        Armv7aRuntimeTrapOrigin origin,
        kernel::TrapOrigin& out) noexcept
    {
        switch (origin) {
        case Armv7aRuntimeTrapOrigin::kernel_thread:
            out = kernel::TrapOrigin::kernel_thread;
            return true;
        case Armv7aRuntimeTrapOrigin::user_task:
            out = kernel::TrapOrigin::user_task;
            return true;
        case Armv7aRuntimeTrapOrigin::supervisor:
            out = kernel::TrapOrigin::supervisor;
            return true;
        case Armv7aRuntimeTrapOrigin::isr:
            out = kernel::TrapOrigin::isr;
            return true;
        case Armv7aRuntimeTrapOrigin::unknown:
        default:
            return false;
        }
    }

    [[nodiscard]] constexpr Armv7aRuntimeTrapIngressDisposition
    map_armv7a_ingress_disposition(kernel::TrapDisposition disposition) noexcept
    {
        switch (disposition) {
        case kernel::TrapDisposition::handled:
            return Armv7aRuntimeTrapIngressDisposition::handled;
        case kernel::TrapDisposition::unsupported:
            return Armv7aRuntimeTrapIngressDisposition::unsupported;
        case kernel::TrapDisposition::rejected:
        default:
            return Armv7aRuntimeTrapIngressDisposition::rejected;
        }
    }

    [[nodiscard]] constexpr Armv7aRuntimeTrapIngressError
    map_armv7a_ingress_error(kernel::TrapError error) noexcept
    {
        switch (error) {
        case kernel::TrapError::none:
            return Armv7aRuntimeTrapIngressError::none;
        case kernel::TrapError::decode_failed:
            return Armv7aRuntimeTrapIngressError::decode_failed;
        case kernel::TrapError::writeback_failed:
            return Armv7aRuntimeTrapIngressError::writeback_failed;
        case kernel::TrapError::unsupported_service:
            return Armv7aRuntimeTrapIngressError::unsupported_service;
        case kernel::TrapError::unbound_adapter:
        case kernel::TrapError::unbound_bridge:
            return Armv7aRuntimeTrapIngressError::unbound_adapter;
        case kernel::TrapError::no_current_task:
        case kernel::TrapError::invalid_origin:
        case kernel::TrapError::invalid_argument:
        default:
            return Armv7aRuntimeTrapIngressError::unsupported_service;
        }
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

    bool capture_armv7a_kernel_trap_frame(
        void* ctx,
        const Armv7aRuntimeTrapLiveFrame& frame,
        kernel::TrapFrameView& out) noexcept
    {
        auto* context = static_cast<Armv7aKernelTrapAdapterContext*>(ctx);
        if (context == nullptr ||
            !armv7a_runtime_trap_frame_adapter_ready(context->lower)) {
            return false;
        }

        Armv7aRuntimeTrapSeamFrameView seam{};
        if (!context->lower.capture(context->lower.ctx, frame, seam)) {
            return false;
        }

        kernel::TrapOrigin origin{};
        if (!map_armv7a_origin(seam.origin, origin)) {
            return false;
        }

        out = kernel::TrapFrameView{
            .service_id = seam.service_id,
            .arg0 = seam.arg0,
            .arg1 = seam.arg1,
            .arg2 = seam.arg2,
            .arg3 = seam.arg3,
            .return_pc = seam.return_pc,
            .stack_pointer = seam.stack_pointer,
            .status = seam.status,
            .origin = origin,
            .task_valid = seam.task_valid,
        };
        out.task.value = static_cast<std::size_t>(seam.task);
        return true;
    }

    bool apply_armv7a_kernel_trap_result(
        void* ctx,
        Armv7aRuntimeTrapLiveFrame& frame,
        const kernel::TrapResult& result) noexcept
    {
        auto* context = static_cast<Armv7aKernelTrapAdapterContext*>(ctx);
        if (context == nullptr ||
            !armv7a_runtime_trap_frame_adapter_ready(context->lower)) {
            return false;
        }

        return context->lower.apply_result(
            context->lower.ctx,
            frame,
            Armv7aRuntimeTrapIngressResult{
                .disposition =
                    map_armv7a_ingress_disposition(result.disposition),
                .error = map_armv7a_ingress_error(result.error),
                .value = result.value,
            });
    }

    [[nodiscard]] auto make_kernel_runtime_trap_frame_adapter(
        Armv7aKernelTrapAdapterContext& context) noexcept
        -> kernel::RuntimeTrapFrameAdapter<Armv7aRuntimeTrapLiveFrame>
    {
        return kernel::RuntimeTrapFrameAdapter<Armv7aRuntimeTrapLiveFrame>{
            .ctx = &context,
            .capture = &capture_armv7a_kernel_trap_frame,
            .apply_result = &apply_armv7a_kernel_trap_result,
        };
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
        Armv7aKernelTrapAdapterContext wrapper{
            .lower = lower_adapter,
        };
        auto generic_adapter = make_kernel_runtime_trap_frame_adapter(wrapper);

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
        Armv7aKernelTrapAdapterContext wrapper{
            .lower = lower_adapter,
        };
        auto generic_adapter = make_kernel_runtime_trap_frame_adapter(wrapper);
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
        Armv7aKernelTrapAdapterContext wrapper{
            .lower = lower_adapter,
        };
        auto generic_adapter = make_kernel_runtime_trap_frame_adapter(wrapper);

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
} // namespace demo

int main()
{
    const bool generic = demo::probe_generic_trap_adapter_capture();
    const bool ingress = demo::probe_task_syscall_ingress_adapter();
    const bool bridge = demo::probe_task_syscall_bridge();
    const bool ok = generic && ingress && bridge;

    std::printf("ok=%d generic=%d ingress=%d bridge=%d\n",
                ok ? 1 : 0,
                generic ? 1 : 0,
                ingress ? 1 : 0,
                bridge ? 1 : 0);
    return ok ? 0 : 1;
}

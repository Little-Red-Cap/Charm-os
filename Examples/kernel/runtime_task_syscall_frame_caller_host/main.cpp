#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_syscall_api;
import kernel.task_syscall_frame;

namespace demo {
    using namespace std::literals;

    struct FakeDispatchSurfaceState {
        bool bound{true};
        std::uint32_t yield_calls{0};
        std::uint32_t sleep_calls{0};
        std::uint32_t debug_calls{0};
        std::uint32_t capability_calls{0};
        std::uint64_t last_due{0};
        std::uint64_t last_capability_id{0};
        std::uint64_t last_capability_operation{0};
        std::uint64_t last_capability_payload{0};
    };

    struct FakeDispatchSurface {
        using tick_type = std::uint64_t;

        FakeDispatchSurfaceState* state{nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return state != nullptr && state->bound;
        }

        [[nodiscard]] kernel::TrapResult yield_current(
            kernel::TrapYieldCurrentView) const noexcept
        {
            if (!valid()) {
                return unbound_bridge_result();
            }

            ++state->yield_calls;
            return handled_result(1u);
        }

        [[nodiscard]] kernel::TrapResult sleep_current_until(
            kernel::TrapSleepUntilView<tick_type> sleep) const noexcept
        {
            if (!valid()) {
                return unbound_bridge_result();
            }

            ++state->sleep_calls;
            state->last_due = sleep.due;
            return handled_result(sleep.due);
        }

        [[nodiscard]] kernel::TrapResult debug_write(
            kernel::TrapDebugWriteView) const noexcept
        {
            if (!valid()) {
                return unbound_bridge_result();
            }

            ++state->debug_calls;
            return handled_result(0u);
        }

        [[nodiscard]] kernel::TrapResult capability_call(
            kernel::TrapCapabilityCallView capability) const noexcept
        {
            if (!valid()) {
                return unbound_bridge_result();
            }

            ++state->capability_calls;
            state->last_capability_id = capability.capability_id;
            state->last_capability_operation = capability.operation;
            state->last_capability_payload = capability.payload;
            return handled_result(capability.capability_id +
                                  capability.operation +
                                  capability.payload);
        }

    private:
        [[nodiscard]] static constexpr kernel::TrapResult handled_result(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }

        [[nodiscard]] static constexpr kernel::TrapResult unbound_bridge_result()
            noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::unbound_bridge,
                .value = 0,
            };
        }
    };

    struct DirectDebugHandlerState {
        std::uint32_t calls{0};
        std::uint64_t last_value{0};
    };

    struct DirectDebugHandler {
        DirectDebugHandlerState* state{nullptr};

        [[nodiscard]] kernel::TrapResult dispatch(
            kernel::TaskSyscallRequest request) const noexcept
        {
            if (state == nullptr) {
                return kernel::TrapResult{
                    .disposition = kernel::TrapDisposition::rejected,
                    .error = kernel::TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            if (request.syscall != kernel::TaskSyscallId::debug_write) {
                return kernel::TrapResult{
                    .disposition = kernel::TrapDisposition::rejected,
                    .error = kernel::TrapError::invalid_argument,
                    .value = 0,
                };
            }

            ++state->calls;
            state->last_value = request.arg0;
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = request.arg0,
            };
        }
    };

    struct FakeFrameAdapterState {
        std::uint32_t capture_calls{0};
        std::uint32_t writeback_calls{0};
    };

    struct FakeTrapFrameAdapterState {
        std::uint32_t capture_calls{0};
        std::uint32_t writeback_calls{0};
    };

    struct FakeCallAdapterState {
        bool build_enabled{true};
        bool ready_enabled{true};
        std::uint32_t make_frame_calls{0};
        std::uint32_t ready_calls{0};
        kernel::TaskSyscallId last_syscall{kernel::TaskSyscallId::invalid};
        std::uint16_t last_service_id{0};
        kernel::TrapOrigin last_origin{kernel::TrapOrigin::kernel_thread};
        std::uint64_t last_arg0{0};
        std::uint64_t last_arg1{0};
        std::uint64_t last_arg2{0};
        std::uint64_t last_arg3{0};
        std::uint64_t last_value{0};
        kernel::TrapError last_error{kernel::TrapError::none};
        bool last_frame_writeback_seen{false};
    };

    struct FakeSyscallFrame {
        std::uint16_t syscall_id{0};
        std::uint64_t arg0{0};
        std::uint64_t arg1{0};
        std::uint64_t arg2{0};
        std::uint64_t arg3{0};
        std::uint64_t return_value{0};
        kernel::TrapDisposition disposition{
            kernel::TrapDisposition::rejected};
        kernel::TrapError error{kernel::TrapError::none};
        bool writeback_seen{false};
    };

    struct FakeTrapFrame {
        std::uint16_t service_id{0};
        std::uint64_t arg0{0};
        std::uint64_t arg1{0};
        std::uint64_t arg2{0};
        std::uint64_t arg3{0};
        std::uint64_t return_value{0};
        kernel::TrapDisposition disposition{
            kernel::TrapDisposition::rejected};
        kernel::TrapError error{kernel::TrapError::none};
        kernel::TrapOrigin origin{kernel::TrapOrigin::user_task};
        bool writeback_seen{false};
    };

    bool capture_fake_syscall_frame(void* ctx,
                                    const FakeSyscallFrame& frame,
                                    kernel::TaskSyscallFrameView& out) noexcept
    {
        auto* state = static_cast<FakeFrameAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->capture_calls;
        }

        out = kernel::TaskSyscallFrameView{
            .syscall = static_cast<kernel::TaskSyscallId>(frame.syscall_id),
            .arg0 = frame.arg0,
            .arg1 = frame.arg1,
            .arg2 = frame.arg2,
            .arg3 = frame.arg3,
        };
        return true;
    }

    bool apply_fake_syscall_result(void* ctx,
                                   FakeSyscallFrame& frame,
                                   const kernel::TrapResult& result) noexcept
    {
        auto* state = static_cast<FakeFrameAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->writeback_calls;
        }

        frame.return_value = result.value;
        frame.disposition = result.disposition;
        frame.error = result.error;
        frame.writeback_seen = true;
        return true;
    }

    bool capture_fake_trap_frame(void* ctx,
                                 const FakeTrapFrame& frame,
                                 kernel::TrapFrameView& out) noexcept
    {
        auto* state = static_cast<FakeTrapFrameAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->capture_calls;
        }

        out = kernel::TrapFrameView{
            .service_id = frame.service_id,
            .arg0 = frame.arg0,
            .arg1 = frame.arg1,
            .arg2 = frame.arg2,
            .arg3 = frame.arg3,
            .origin = frame.origin,
        };
        return true;
    }

    bool apply_fake_trap_result(void* ctx,
                                FakeTrapFrame& frame,
                                const kernel::TrapResult& result) noexcept
    {
        auto* state = static_cast<FakeTrapFrameAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->writeback_calls;
        }

        frame.return_value = result.value;
        frame.disposition = result.disposition;
        frame.error = result.error;
        frame.writeback_seen = true;
        return true;
    }

    bool make_fake_call_frame(void* ctx,
                              kernel::TaskSyscallRequest request,
                              FakeSyscallFrame& out) noexcept
    {
        auto* state = static_cast<FakeCallAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->make_frame_calls;
            state->last_syscall = request.syscall;
            state->last_arg0 = request.arg0;
            state->last_arg1 = request.arg1;
            state->last_arg2 = request.arg2;
            state->last_arg3 = request.arg3;
            if (!state->build_enabled) {
                return false;
            }
        }

        out = FakeSyscallFrame{
            .syscall_id = static_cast<std::uint16_t>(request.syscall),
            .arg0 = request.arg0,
            .arg1 = request.arg1,
            .arg2 = request.arg2,
            .arg3 = request.arg3,
        };
        return true;
    }

    bool make_fake_trap_request_frame(void* ctx,
                                      kernel::TrapRequest request,
                                      FakeTrapFrame& out) noexcept
    {
        auto* state = static_cast<FakeCallAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->make_frame_calls;
            state->last_syscall =
                kernel::task_syscall_from_trap_service(request.service);
            state->last_service_id = static_cast<std::uint16_t>(request.service);
            state->last_origin = request.origin;
            state->last_arg0 = request.arg0;
            state->last_arg1 = request.arg1;
            state->last_arg2 = request.arg2;
            state->last_arg3 = request.arg3;
            if (!state->build_enabled) {
                return false;
            }
        }

        out = FakeTrapFrame{
            .service_id = static_cast<std::uint16_t>(request.service),
            .arg0 = request.arg0,
            .arg1 = request.arg1,
            .arg2 = request.arg2,
            .arg3 = request.arg3,
            .origin = request.origin,
        };
        return true;
    }

    bool fake_call_result_ready(void* ctx,
                                const FakeSyscallFrame& frame,
                                const kernel::TrapResult& result) noexcept
    {
        auto* state = static_cast<FakeCallAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->ready_calls;
            state->last_value = result.value;
            state->last_error = result.error;
            state->last_frame_writeback_seen = frame.writeback_seen;
            if (!state->ready_enabled) {
                return false;
            }
        }

        return frame.writeback_seen && frame.return_value == result.value &&
               frame.error == result.error &&
               frame.disposition == result.disposition;
    }

    bool fake_trap_call_result_ready(void* ctx,
                                     const FakeTrapFrame& frame,
                                     const kernel::TrapResult& result) noexcept
    {
        auto* state = static_cast<FakeCallAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->ready_calls;
            state->last_value = result.value;
            state->last_error = result.error;
            state->last_frame_writeback_seen = frame.writeback_seen;
            if (!state->ready_enabled) {
                return false;
            }
        }

        return frame.writeback_seen && frame.return_value == result.value &&
               frame.error == result.error &&
               frame.disposition == result.disposition;
    }

    using DispatchTrace = kernel::TaskSyscallDispatchTraceBuffer<8>;
    using DispatchBridge =
        kernel::TaskSyscallDispatcher<FakeDispatchSurface, DispatchTrace>;
    using TableTrace = kernel::TaskSyscallTableTraceBuffer<8>;
    using StaticTable = kernel::TaskSyscallTable<4, TableTrace>;
    using FrameTrace = kernel::TaskSyscallFrameTraceBuffer<16>;
    using FrameBridge =
        kernel::TaskSyscallFrameBridge<StaticTable, FakeSyscallFrame, FrameTrace>;
    using FrameCaller =
        kernel::TaskSyscallFrameCaller<FakeSyscallFrame, std::uint64_t>;
    using TrapFrameCaller =
        kernel::TaskSyscallFrameCaller<FakeTrapFrame, std::uint64_t>;
    using TaskSyscalls = kernel::TaskSyscallApi<FrameCaller>;
    using TrapTaskSyscalls = kernel::TaskSyscallApi<TrapFrameCaller>;

    [[nodiscard]] constexpr bool same_text(const char* actual,
                                           std::string_view expected) noexcept
    {
        return actual != nullptr && std::string_view{actual} == expected;
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

    [[nodiscard]] bool probe_syscall_api_over_frame_stack() noexcept
    {
        FakeDispatchSurfaceState dispatch_state{};
        DirectDebugHandlerState debug_state{};
        FakeFrameAdapterState frame_adapter_state{};
        FakeCallAdapterState call_adapter_state{};
        DispatchTrace dispatch_trace{};
        TableTrace table_trace{};
        FrameTrace frame_trace{};
        DispatchBridge dispatch_bridge{FakeDispatchSurface{
                                           .state = &dispatch_state,
                                       },
                                       &dispatch_trace};
        DirectDebugHandler debug_handler{
            .state = &debug_state,
        };
        auto table = kernel::make_task_syscall_table(
            std::array<kernel::TaskSyscallHandlerEntry, 4>{
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::yield,
                    kernel::make_task_syscall_handler(dispatch_bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::sleep_until,
                    kernel::make_task_syscall_handler(dispatch_bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::debug_write,
                    kernel::make_task_syscall_handler(debug_handler)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::capability_call,
                    kernel::make_task_syscall_handler(dispatch_bridge)),
            },
            &table_trace);
        auto frame_bridge = kernel::make_task_syscall_frame_bridge(
            table,
            kernel::TaskSyscallFrameAdapter<FakeSyscallFrame>{
                .ctx = &frame_adapter_state,
                .capture = &capture_fake_syscall_frame,
                .apply_result = &apply_fake_syscall_result,
            },
            &frame_trace);
        auto caller = kernel::make_task_syscall_frame_caller<FakeSyscallFrame,
                                                             std::uint64_t>(
            kernel::make_task_syscall_frame_port(frame_bridge),
            kernel::TaskSyscallCallFrameAdapter<FakeSyscallFrame>{
                .ctx = &call_adapter_state,
                .make_frame = &make_fake_call_frame,
                .result_ready = &fake_call_result_ready,
            });
        auto syscalls = kernel::make_task_syscall_api(caller);

        const auto yielded = syscalls.sys_yield();
        const auto yielded_view =
            syscalls.sys_yield(kernel::TrapYieldCurrentView{});
        const auto slept = syscalls.sys_sleep_until(55u);
        const auto debugged = syscalls.sys_debug_write(
            kernel::TrapDebugWriteView{
                .value = 0xCCu,
            });
        const auto called = syscalls.sys_capability_call(
            kernel::TrapCapabilityCallView{
                .capability_id = 7u,
                .operation = 2u,
                .payload = 33u,
            });

        const auto* first = frame_trace.at(0u);
        const auto* second = frame_trace.at(1u);
        const auto* fifteenth = frame_trace.at(14u);
        if (first == nullptr || second == nullptr || fifteenth == nullptr) {
            return false;
        }

        const auto called_projection =
            kernel::task_syscall_semantic_projection(*fifteenth);

        return syscalls.valid() && syscalls.runtime().valid() &&
               trap_result_matches(yielded,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               trap_result_matches(yielded_view,
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
                                   0xCCu) &&
               trap_result_matches(called,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               dispatch_state.yield_calls == 2u &&
               dispatch_state.sleep_calls == 1u &&
               dispatch_state.debug_calls == 0u &&
               dispatch_state.capability_calls == 1u &&
               dispatch_state.last_due == 55u &&
               dispatch_state.last_capability_id == 7u &&
               dispatch_state.last_capability_operation == 2u &&
               dispatch_state.last_capability_payload == 33u &&
               debug_state.calls == 1u &&
               debug_state.last_value == 0xCCu &&
               frame_adapter_state.capture_calls == 5u &&
               frame_adapter_state.writeback_calls == 5u &&
               call_adapter_state.make_frame_calls == 5u &&
               call_adapter_state.ready_calls == 5u &&
               call_adapter_state.last_syscall ==
                   kernel::TaskSyscallId::capability_call &&
               call_adapter_state.last_arg0 == 7u &&
               call_adapter_state.last_arg1 == 2u &&
               call_adapter_state.last_arg2 == 33u &&
               call_adapter_state.last_arg3 == 0u &&
               call_adapter_state.last_value == 42u &&
               call_adapter_state.last_error == kernel::TrapError::none &&
               call_adapter_state.last_frame_writeback_seen &&
               dispatch_trace.size() == 4u &&
               table_trace.size() == 5u &&
               frame_trace.size() == 15u &&
               first->sequence == 1u &&
               first->stage == kernel::TaskSyscallFrameStage::decode &&
               same_text(kernel::task_syscall_frame_stage_name(first->stage),
                         "decode"sv) &&
               first->syscall == kernel::TaskSyscallId::yield &&
               second->sequence == 2u &&
               second->stage == kernel::TaskSyscallFrameStage::dispatch &&
               second->value == 1u &&
               fifteenth->sequence == 15u &&
               fifteenth->stage == kernel::TaskSyscallFrameStage::writeback &&
               fifteenth->syscall == kernel::TaskSyscallId::capability_call &&
               fifteenth->value == 42u && fifteenth->ok &&
               same_text(called_projection.descriptor.syscall_name,
                         "capability-call"sv) &&
               called_projection.field_count == 3u &&
               same_text(called_projection.fields[0].name,
                         "capability-id"sv) &&
               called_projection.fields[0].value == 7u &&
               same_text(called_projection.fields[1].name, "operation"sv) &&
               called_projection.fields[1].value == 2u &&
               same_text(called_projection.fields[2].name, "payload"sv) &&
               called_projection.fields[2].value == 33u;
    }

    [[nodiscard]] bool probe_syscall_api_over_trap_frame_stack() noexcept
    {
        FakeDispatchSurfaceState dispatch_state{};
        DirectDebugHandlerState debug_state{};
        FakeTrapFrameAdapterState trap_adapter_state{};
        FakeCallAdapterState call_adapter_state{};
        DispatchTrace dispatch_trace{};
        TableTrace table_trace{};
        FrameTrace frame_trace{};
        DispatchBridge dispatch_bridge{FakeDispatchSurface{
                                           .state = &dispatch_state,
                                       },
                                       &dispatch_trace};
        DirectDebugHandler debug_handler{
            .state = &debug_state,
        };
        auto table = kernel::make_task_syscall_table(
            std::array<kernel::TaskSyscallHandlerEntry, 4>{
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::yield,
                    kernel::make_task_syscall_handler(dispatch_bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::sleep_until,
                    kernel::make_task_syscall_handler(dispatch_bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::debug_write,
                    kernel::make_task_syscall_handler(debug_handler)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::capability_call,
                    kernel::make_task_syscall_handler(dispatch_bridge)),
            },
            &table_trace);
        auto trap_adapter = kernel::RuntimeTrapFrameAdapter<FakeTrapFrame>{
            .ctx = &trap_adapter_state,
            .capture = &capture_fake_trap_frame,
            .apply_result = &apply_fake_trap_result,
        };
        auto trap_call_adapter =
            kernel::TaskSyscallTrapCallFrameAdapter<FakeTrapFrame>{
                .ctx = &call_adapter_state,
                .origin = kernel::TrapOrigin::user_task,
                .make_frame = &make_fake_trap_request_frame,
                .result_ready = &fake_trap_call_result_ready,
            };
        auto generic_call_adapter =
            kernel::make_task_syscall_call_frame_adapter(trap_call_adapter);
        auto frame_bridge = kernel::make_task_syscall_frame_bridge(
            table, trap_adapter, &frame_trace);
        auto caller = kernel::make_task_syscall_frame_caller<FakeTrapFrame,
                                                             std::uint64_t>(
            kernel::make_task_syscall_frame_port(frame_bridge),
            trap_call_adapter);
        TrapTaskSyscalls syscalls{caller};

        const auto yielded = syscalls.sys_yield();
        const auto slept = syscalls.sys_sleep_until(55u);
        const auto debugged = syscalls.sys_debug_write(
            kernel::TrapDebugWriteView{
                .value = 0xCCu,
            });
        const auto called = syscalls.sys_capability_call(
            kernel::TrapCapabilityCallView{
                .capability_id = 7u,
                .operation = 2u,
                .payload = 33u,
            });

        const auto* first = frame_trace.at(0u);
        const auto* sixth = frame_trace.at(5u);
        const auto* twelfth = frame_trace.at(11u);
        if (first == nullptr || sixth == nullptr || twelfth == nullptr) {
            return false;
        }

        const auto called_projection =
            kernel::task_syscall_semantic_projection(*twelfth);

        return kernel::task_syscall_trap_call_frame_adapter_ready(
                   trap_call_adapter) &&
               kernel::task_syscall_call_frame_adapter_ready(
                   generic_call_adapter) &&
               syscalls.valid() && syscalls.runtime().valid() &&
               trap_result_matches(yielded,
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
                                   0xCCu) &&
               trap_result_matches(called,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               dispatch_state.yield_calls == 1u &&
               dispatch_state.sleep_calls == 1u &&
               dispatch_state.debug_calls == 0u &&
               dispatch_state.capability_calls == 1u &&
               dispatch_state.last_due == 55u &&
               dispatch_state.last_capability_id == 7u &&
               dispatch_state.last_capability_operation == 2u &&
               dispatch_state.last_capability_payload == 33u &&
               debug_state.calls == 1u &&
               debug_state.last_value == 0xCCu &&
               trap_adapter_state.capture_calls == 4u &&
               trap_adapter_state.writeback_calls == 4u &&
               call_adapter_state.make_frame_calls == 4u &&
               call_adapter_state.ready_calls == 4u &&
               call_adapter_state.last_syscall ==
                   kernel::TaskSyscallId::capability_call &&
               call_adapter_state.last_service_id ==
                   static_cast<std::uint16_t>(
                       kernel::TrapService::capability_call) &&
               call_adapter_state.last_origin ==
                   kernel::TrapOrigin::user_task &&
               call_adapter_state.last_arg0 == 7u &&
               call_adapter_state.last_arg1 == 2u &&
               call_adapter_state.last_arg2 == 33u &&
               call_adapter_state.last_arg3 == 0u &&
               call_adapter_state.last_value == 42u &&
               call_adapter_state.last_error == kernel::TrapError::none &&
               call_adapter_state.last_frame_writeback_seen &&
               dispatch_trace.size() == 3u &&
               table_trace.size() == 4u &&
               frame_trace.size() == 12u &&
               first->sequence == 1u &&
               first->stage == kernel::TaskSyscallFrameStage::decode &&
               same_text(kernel::task_syscall_frame_stage_name(first->stage),
                         "decode"sv) &&
               first->syscall == kernel::TaskSyscallId::yield &&
               sixth->sequence == 6u &&
               sixth->stage == kernel::TaskSyscallFrameStage::writeback &&
               sixth->syscall == kernel::TaskSyscallId::sleep_until &&
               sixth->value == 55u && sixth->ok &&
               twelfth->sequence == 12u &&
               twelfth->stage == kernel::TaskSyscallFrameStage::writeback &&
               twelfth->syscall == kernel::TaskSyscallId::capability_call &&
               twelfth->value == 42u && twelfth->ok &&
               same_text(called_projection.descriptor.syscall_name,
                         "capability-call"sv) &&
               called_projection.field_count == 3u &&
               same_text(called_projection.fields[0].name,
                         "capability-id"sv) &&
               called_projection.fields[0].value == 7u &&
               same_text(called_projection.fields[1].name, "operation"sv) &&
               called_projection.fields[1].value == 2u &&
               same_text(called_projection.fields[2].name, "payload"sv) &&
               called_projection.fields[2].value == 33u;
    }

    [[nodiscard]] bool probe_bind_runtime() noexcept
    {
        FakeDispatchSurfaceState first_dispatch_state{};
        DirectDebugHandlerState first_debug_state{};
        FakeFrameAdapterState first_frame_adapter_state{};
        FakeCallAdapterState first_call_adapter_state{};
        DispatchTrace first_dispatch_trace{};
        TableTrace first_table_trace{};
        FrameTrace first_frame_trace{};
        DispatchBridge first_dispatch_bridge{FakeDispatchSurface{
                                                 .state = &first_dispatch_state,
                                             },
                                             &first_dispatch_trace};
        DirectDebugHandler first_debug_handler{
            .state = &first_debug_state,
        };
        auto first_table = kernel::make_task_syscall_table(
            std::array<kernel::TaskSyscallHandlerEntry, 4>{
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::yield,
                    kernel::make_task_syscall_handler(first_dispatch_bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::sleep_until,
                    kernel::make_task_syscall_handler(first_dispatch_bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::debug_write,
                    kernel::make_task_syscall_handler(first_debug_handler)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::capability_call,
                    kernel::make_task_syscall_handler(first_dispatch_bridge)),
            },
            &first_table_trace);
        auto first_frame_bridge = kernel::make_task_syscall_frame_bridge(
            first_table,
            kernel::TaskSyscallFrameAdapter<FakeSyscallFrame>{
                .ctx = &first_frame_adapter_state,
                .capture = &capture_fake_syscall_frame,
                .apply_result = &apply_fake_syscall_result,
            },
            &first_frame_trace);
        auto first_caller = kernel::make_task_syscall_frame_caller<
            FakeSyscallFrame,
            std::uint64_t>(
            kernel::make_task_syscall_frame_port(first_frame_bridge),
            kernel::TaskSyscallCallFrameAdapter<FakeSyscallFrame>{
                .ctx = &first_call_adapter_state,
                .make_frame = &make_fake_call_frame,
                .result_ready = &fake_call_result_ready,
            });

        FakeDispatchSurfaceState second_dispatch_state{};
        DirectDebugHandlerState second_debug_state{};
        FakeFrameAdapterState second_frame_adapter_state{};
        FakeCallAdapterState second_call_adapter_state{};
        DispatchTrace second_dispatch_trace{};
        TableTrace second_table_trace{};
        FrameTrace second_frame_trace{};
        DispatchBridge second_dispatch_bridge{FakeDispatchSurface{
                                                  .state = &second_dispatch_state,
                                              },
                                              &second_dispatch_trace};
        DirectDebugHandler second_debug_handler{
            .state = &second_debug_state,
        };
        auto second_table = kernel::make_task_syscall_table(
            std::array<kernel::TaskSyscallHandlerEntry, 4>{
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::yield,
                    kernel::make_task_syscall_handler(second_dispatch_bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::sleep_until,
                    kernel::make_task_syscall_handler(second_dispatch_bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::debug_write,
                    kernel::make_task_syscall_handler(second_debug_handler)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::capability_call,
                    kernel::make_task_syscall_handler(second_dispatch_bridge)),
            },
            &second_table_trace);
        auto second_frame_bridge = kernel::make_task_syscall_frame_bridge(
            second_table,
            kernel::TaskSyscallFrameAdapter<FakeSyscallFrame>{
                .ctx = &second_frame_adapter_state,
                .capture = &capture_fake_syscall_frame,
                .apply_result = &apply_fake_syscall_result,
            },
            &second_frame_trace);
        auto second_caller = kernel::make_task_syscall_frame_caller<
            FakeSyscallFrame,
            std::uint64_t>(
            kernel::make_task_syscall_frame_port(second_frame_bridge),
            kernel::TaskSyscallCallFrameAdapter<FakeSyscallFrame>{
                .ctx = &second_call_adapter_state,
                .make_frame = &make_fake_call_frame,
                .result_ready = &fake_call_result_ready,
            });

        TaskSyscalls syscalls{first_caller};
        const auto first_yield = syscalls.sys_yield();
        syscalls.bind_runtime(FrameCaller{});
        const bool unbound_valid = syscalls.valid();
        const auto unbound_sleep = syscalls.sys_sleep_until(9u);
        syscalls.bind_runtime(second_caller);
        const auto rebound_capability = syscalls.sys_capability_call(5u, 6u);

        return trap_result_matches(first_yield,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               !unbound_valid &&
               trap_result_matches(unbound_sleep,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_adapter) &&
               syscalls.valid() && syscalls.runtime().valid() &&
               trap_result_matches(rebound_capability,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   11u) &&
               first_dispatch_state.yield_calls == 1u &&
               second_dispatch_state.capability_calls == 1u &&
               second_dispatch_state.last_capability_id == 5u &&
               second_dispatch_state.last_capability_operation == 6u &&
               second_dispatch_state.last_capability_payload == 0u &&
               second_call_adapter_state.last_value == 11u &&
               second_call_adapter_state.last_frame_writeback_seen;
    }

    [[nodiscard]] bool probe_caller_negative_paths() noexcept
    {
        {
            TaskSyscalls syscalls{};
            const auto unbound = syscalls.sys_yield();
            if (!trap_result_matches(unbound,
                                     kernel::TrapDisposition::rejected,
                                     kernel::TrapError::unbound_adapter)) {
                return false;
            }
        }

        {
            FakeDispatchSurfaceState dispatch_state{};
            DirectDebugHandlerState debug_state{};
            FakeFrameAdapterState frame_adapter_state{};
            FakeCallAdapterState call_adapter_state{
                .build_enabled = false,
            };
            DispatchTrace dispatch_trace{};
            TableTrace table_trace{};
            FrameTrace frame_trace{};
            DispatchBridge dispatch_bridge{FakeDispatchSurface{
                                               .state = &dispatch_state,
                                           },
                                           &dispatch_trace};
            DirectDebugHandler debug_handler{
                .state = &debug_state,
            };
            auto table = kernel::make_task_syscall_table(
                std::array<kernel::TaskSyscallHandlerEntry, 4>{
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::yield,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::sleep_until,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::debug_write,
                        kernel::make_task_syscall_handler(debug_handler)),
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::capability_call,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                },
                &table_trace);
            auto frame_bridge = kernel::make_task_syscall_frame_bridge(
                table,
                kernel::TaskSyscallFrameAdapter<FakeSyscallFrame>{
                    .ctx = &frame_adapter_state,
                    .capture = &capture_fake_syscall_frame,
                    .apply_result = &apply_fake_syscall_result,
                },
                &frame_trace);
            auto caller = kernel::make_task_syscall_frame_caller<
                FakeSyscallFrame,
                std::uint64_t>(
                kernel::make_task_syscall_frame_port(frame_bridge),
                kernel::TaskSyscallCallFrameAdapter<FakeSyscallFrame>{
                    .ctx = &call_adapter_state,
                    .make_frame = &make_fake_call_frame,
                    .result_ready = &fake_call_result_ready,
                });
            TaskSyscalls syscalls{caller};
            const auto failed = syscalls.sys_debug_write(0x77u);

            if (!trap_result_matches(failed,
                                     kernel::TrapDisposition::rejected,
                                     kernel::TrapError::decode_failed) ||
                call_adapter_state.make_frame_calls != 1u ||
                call_adapter_state.ready_calls != 0u ||
                dispatch_trace.size() != 0u || table_trace.size() != 0u ||
                frame_trace.size() != 0u || debug_state.calls != 0u ||
                frame_adapter_state.capture_calls != 0u ||
                frame_adapter_state.writeback_calls != 0u) {
                return false;
            }
        }

        {
            FakeDispatchSurfaceState dispatch_state{};
            DirectDebugHandlerState debug_state{};
            FakeTrapFrameAdapterState trap_adapter_state{};
            FakeCallAdapterState call_adapter_state{
                .build_enabled = false,
            };
            DispatchTrace dispatch_trace{};
            TableTrace table_trace{};
            FrameTrace frame_trace{};
            DispatchBridge dispatch_bridge{FakeDispatchSurface{
                                               .state = &dispatch_state,
                                           },
                                           &dispatch_trace};
            DirectDebugHandler debug_handler{
                .state = &debug_state,
            };
            auto table = kernel::make_task_syscall_table(
                std::array<kernel::TaskSyscallHandlerEntry, 4>{
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::yield,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::sleep_until,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::debug_write,
                        kernel::make_task_syscall_handler(debug_handler)),
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::capability_call,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                },
                &table_trace);
            auto trap_adapter = kernel::RuntimeTrapFrameAdapter<FakeTrapFrame>{
                .ctx = &trap_adapter_state,
                .capture = &capture_fake_trap_frame,
                .apply_result = &apply_fake_trap_result,
            };
            auto frame_bridge = kernel::make_task_syscall_frame_bridge(
                table, trap_adapter, &frame_trace);
            auto trap_call_adapter =
                kernel::TaskSyscallTrapCallFrameAdapter<FakeTrapFrame>{
                    .ctx = &call_adapter_state,
                    .origin = kernel::TrapOrigin::user_task,
                    .make_frame = &make_fake_trap_request_frame,
                    .result_ready = &fake_trap_call_result_ready,
                };
            auto caller = kernel::make_task_syscall_frame_caller<
                FakeTrapFrame,
                std::uint64_t>(
                kernel::make_task_syscall_frame_port(frame_bridge),
                trap_call_adapter);
            TrapTaskSyscalls syscalls{caller};
            const auto failed = syscalls.sys_debug_write(0x88u);

            if (!trap_result_matches(failed,
                                     kernel::TrapDisposition::rejected,
                                     kernel::TrapError::decode_failed) ||
                call_adapter_state.make_frame_calls != 1u ||
                call_adapter_state.ready_calls != 0u ||
                call_adapter_state.last_syscall !=
                    kernel::TaskSyscallId::debug_write ||
                call_adapter_state.last_service_id !=
                    static_cast<std::uint16_t>(
                        kernel::TrapService::debug_write) ||
                call_adapter_state.last_origin !=
                    kernel::TrapOrigin::user_task ||
                call_adapter_state.last_arg0 != 0x88u ||
                call_adapter_state.last_arg1 != 0u ||
                call_adapter_state.last_arg2 != 0u ||
                call_adapter_state.last_arg3 != 0u ||
                dispatch_trace.size() != 0u || table_trace.size() != 0u ||
                frame_trace.size() != 0u || debug_state.calls != 0u ||
                trap_adapter_state.capture_calls != 0u ||
                trap_adapter_state.writeback_calls != 0u) {
                return false;
            }
        }

        {
            FakeDispatchSurfaceState dispatch_state{};
            DirectDebugHandlerState debug_state{};
            FakeFrameAdapterState frame_adapter_state{};
            FakeCallAdapterState call_adapter_state{
                .build_enabled = true,
                .ready_enabled = false,
            };
            DispatchTrace dispatch_trace{};
            TableTrace table_trace{};
            FrameTrace frame_trace{};
            DispatchBridge dispatch_bridge{FakeDispatchSurface{
                                               .state = &dispatch_state,
                                           },
                                           &dispatch_trace};
            DirectDebugHandler debug_handler{
                .state = &debug_state,
            };
            auto table = kernel::make_task_syscall_table(
                std::array<kernel::TaskSyscallHandlerEntry, 4>{
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::yield,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::sleep_until,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::debug_write,
                        kernel::make_task_syscall_handler(debug_handler)),
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::capability_call,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                },
                &table_trace);
            auto frame_bridge = kernel::make_task_syscall_frame_bridge(
                table,
                kernel::TaskSyscallFrameAdapter<FakeSyscallFrame>{
                    .ctx = &frame_adapter_state,
                    .capture = &capture_fake_syscall_frame,
                    .apply_result = &apply_fake_syscall_result,
                },
                &frame_trace);
            auto caller = kernel::make_task_syscall_frame_caller<
                FakeSyscallFrame,
                std::uint64_t>(
                kernel::make_task_syscall_frame_port(frame_bridge),
                kernel::TaskSyscallCallFrameAdapter<FakeSyscallFrame>{
                    .ctx = &call_adapter_state,
                    .make_frame = &make_fake_call_frame,
                    .result_ready = &fake_call_result_ready,
                });
            TaskSyscalls syscalls{caller};
            const auto failed = syscalls.sys_capability_call(7u, 2u, 33u);

            if (!trap_result_matches(failed,
                                     kernel::TrapDisposition::rejected,
                                     kernel::TrapError::writeback_failed,
                                     42u) ||
                call_adapter_state.make_frame_calls != 1u ||
                call_adapter_state.ready_calls != 1u ||
                !call_adapter_state.last_frame_writeback_seen ||
                call_adapter_state.last_value != 42u ||
                dispatch_state.capability_calls != 1u ||
                dispatch_state.last_capability_id != 7u ||
                dispatch_state.last_capability_operation != 2u ||
                dispatch_state.last_capability_payload != 33u ||
                table_trace.size() != 1u || dispatch_trace.size() != 1u ||
                frame_trace.size() != 3u ||
                frame_adapter_state.capture_calls != 1u ||
                frame_adapter_state.writeback_calls != 1u ||
                debug_state.calls != 0u) {
                return false;
            }
        }

        return true;
    }
}

int main()
{
    const bool stack_ok = demo::probe_syscall_api_over_frame_stack();
    const bool trap_ok = demo::probe_syscall_api_over_trap_frame_stack();
    const bool bind_ok = demo::probe_bind_runtime();
    const bool error_ok = demo::probe_caller_negative_paths();
    const bool ok = stack_ok && trap_ok && bind_ok && error_ok;

    std::printf(
        "[runtime-task-syscall-frame-caller-demo] ok=%d stack=%d trap=%d bind=%d error=%d\n",
        ok ? 1 : 0,
        stack_ok ? 1 : 0,
        trap_ok ? 1 : 0,
        bind_ok ? 1 : 0,
        error_ok ? 1 : 0);
    return ok ? 0 : 1;
}

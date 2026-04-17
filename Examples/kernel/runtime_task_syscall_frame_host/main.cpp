#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

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
        bool capture_enabled{true};
        bool writeback_enabled{true};
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
        bool capture_enabled{true};
        bool writeback_enabled{true};
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

        if (!frame.capture_enabled) {
            return false;
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

        if (!frame.writeback_enabled) {
            return false;
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

        if (!frame.capture_enabled) {
            return false;
        }

        out = kernel::TrapFrameView{
            .service_id = frame.service_id,
            .arg0 = frame.arg0,
            .arg1 = frame.arg1,
            .arg2 = frame.arg2,
            .arg3 = frame.arg3,
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

        if (!frame.writeback_enabled) {
            return false;
        }

        frame.return_value = result.value;
        frame.disposition = result.disposition;
        frame.error = result.error;
        frame.writeback_seen = true;
        return true;
    }

    using DispatchTrace = kernel::TaskSyscallDispatchTraceBuffer<8>;
    using DispatchBridge =
        kernel::TaskSyscallDispatcher<FakeDispatchSurface, DispatchTrace>;
    using TableTrace = kernel::TaskSyscallTableTraceBuffer<8>;
    using StaticTable = kernel::TaskSyscallTable<4, TableTrace>;
    using FrameTrace = kernel::TaskSyscallFrameTraceBuffer<16>;
    using FrameBridge =
        kernel::TaskSyscallFrameBridge<StaticTable, FakeSyscallFrame, FrameTrace>;
    using FramePort = kernel::TaskSyscallFramePort<FakeSyscallFrame>;

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

    [[nodiscard]] constexpr bool probe_trap_decode_bridge() noexcept
    {
        kernel::TaskSyscallFrameView capability_view{};
        kernel::TaskSyscallFrameView debug_view{};
        kernel::TaskSyscallFrameView invalid_view{};
        const auto capability_ok = kernel::task_syscall_frame_view_decode(
            kernel::make_capability_call_trap_request(
                kernel::TrapCapabilityCallView{
                    .capability_id = 7u,
                    .operation = 2u,
                    .payload = 33u,
                },
                kernel::TrapOrigin::user_task),
            capability_view);
        const auto debug_ok = kernel::task_syscall_frame_view_decode(
            kernel::TrapFrameView{
                .service_id = static_cast<std::uint16_t>(
                    kernel::TrapService::debug_write),
                .arg0 = 0xCCu,
                .origin = kernel::TrapOrigin::kernel_thread,
            },
            debug_view);
        const auto invalid_ok = kernel::task_syscall_frame_view_decode(
            kernel::TrapRequest{
                .service = kernel::TrapService::invalid,
                .arg0 = 1u,
            },
            invalid_view);
        const auto debug_projection =
            kernel::task_syscall_semantic_projection(debug_view);

        return capability_ok && debug_ok && !invalid_ok &&
               kernel::task_syscall_frame_view_ready(capability_view) &&
               kernel::task_syscall_frame_view_ready(debug_view) &&
               !kernel::task_syscall_frame_view_ready(invalid_view) &&
               capability_view.syscall ==
                   kernel::TaskSyscallId::capability_call &&
               capability_view.arg0 == 7u && capability_view.arg1 == 2u &&
               capability_view.arg2 == 33u &&
               debug_view.syscall == kernel::TaskSyscallId::debug_write &&
               debug_view.arg0 == 0xCCu &&
               same_text(debug_projection.descriptor.syscall_name,
                         "debug-write"sv) &&
               debug_projection.field_count == 1u &&
               same_text(debug_projection.fields[0].name, "value"sv) &&
               debug_projection.fields[0].value == 0xCCu;
    }

    [[nodiscard]] constexpr bool probe_frame_view_conversion() noexcept
    {
        const auto view = kernel::TaskSyscallFrameView{
            .syscall = kernel::TaskSyscallId::capability_call,
            .arg0 = 7u,
            .arg1 = 2u,
            .arg2 = 33u,
            .arg3 = 0u,
        };
        const auto request = kernel::task_syscall_request_from_frame_view(view);
        const auto round_trip =
            kernel::task_syscall_frame_view_from_request(request);

        return request.syscall == kernel::TaskSyscallId::capability_call &&
               request.arg0 == 7u && request.arg1 == 2u &&
               request.arg2 == 33u &&
               round_trip.syscall == view.syscall &&
               round_trip.arg0 == view.arg0 && round_trip.arg1 == view.arg1 &&
               round_trip.arg2 == view.arg2 && round_trip.arg3 == view.arg3;
    }

    static_assert(probe_frame_view_conversion());

    [[nodiscard]] bool probe_runtime_trap_adapter_bridge() noexcept
    {
        FakeTrapFrameAdapterState trap_state{};
        auto ingress_adapter = kernel::make_task_syscall_frame_ingress_adapter(
            kernel::RuntimeTrapFrameAdapter<FakeTrapFrame>{
                .ctx = &trap_state,
                .capture = &capture_fake_trap_frame,
                .apply_result = &apply_fake_trap_result,
            });
        auto frame_adapter =
            kernel::make_task_syscall_frame_adapter(ingress_adapter);
        FakeTrapFrame capability_frame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::capability_call),
            .arg0 = 7u,
            .arg1 = 2u,
            .arg2 = 33u,
        };
        FakeTrapFrame invalid_frame{
            .service_id = static_cast<std::uint16_t>(kernel::TrapService::invalid),
            .arg0 = 1u,
        };
        kernel::TaskSyscallFrameView capability_view{};
        kernel::TaskSyscallFrameView invalid_view{};
        const auto captured = ingress_adapter.capture(capability_frame,
                                                      capability_view);
        const auto applied = ingress_adapter.apply_result(
            capability_frame,
            kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = 42u,
            });
        const auto invalid_captured =
            ingress_adapter.capture(invalid_frame, invalid_view);

        FakeTrapFrame debug_frame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::debug_write),
            .arg0 = 0xCCu,
        };
        kernel::TaskSyscallFrameView debug_view{};
        const auto bridged_captured = frame_adapter.capture(
            frame_adapter.ctx, debug_frame, debug_view);
        const auto bridged_applied = frame_adapter.apply_result(
            frame_adapter.ctx,
            debug_frame,
            kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = 0xCCu,
            });
        const auto debug_projection =
            kernel::task_syscall_semantic_projection(debug_view);

        return kernel::task_syscall_frame_ingress_adapter_ready(
                   ingress_adapter) &&
               kernel::task_syscall_frame_adapter_ready(frame_adapter) &&
               captured && applied && !invalid_captured && bridged_captured &&
               bridged_applied &&
               trap_state.capture_calls == 3u &&
               trap_state.writeback_calls == 2u &&
               capability_view.syscall ==
                   kernel::TaskSyscallId::capability_call &&
               capability_view.arg0 == 7u && capability_view.arg1 == 2u &&
               capability_view.arg2 == 33u &&
               capability_frame.writeback_seen &&
               capability_frame.return_value == 42u &&
               debug_view.syscall == kernel::TaskSyscallId::debug_write &&
               debug_view.arg0 == 0xCCu &&
               debug_frame.writeback_seen &&
               debug_frame.return_value == 0xCCu &&
               same_text(debug_projection.descriptor.syscall_name,
                         "debug-write"sv) &&
               debug_projection.field_count == 1u &&
               same_text(debug_projection.fields[0].name, "value"sv) &&
               debug_projection.fields[0].value == 0xCCu;
    }

    [[nodiscard]] bool probe_runtime_trap_bridge_helpers() noexcept
    {
        FakeDispatchSurfaceState dispatch_state{};
        DirectDebugHandlerState debug_state{};
        FakeTrapFrameAdapterState trap_state{};
        DispatchTrace dispatch_trace{};
        TableTrace table_trace{};
        FrameTrace trap_frame_trace{};
        FrameTrace ingress_frame_trace{};
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
            .ctx = &trap_state,
            .capture = &capture_fake_trap_frame,
            .apply_result = &apply_fake_trap_result,
        };
        auto direct_adapter =
            kernel::make_task_syscall_frame_adapter(trap_adapter);
        auto ingress_adapter =
            kernel::make_task_syscall_frame_ingress_adapter(trap_adapter);
        auto trap_bridge = kernel::make_task_syscall_frame_bridge(
            table, trap_adapter, &trap_frame_trace);
        auto ingress_bridge = kernel::make_task_syscall_frame_bridge(
            table, ingress_adapter, &ingress_frame_trace);

        FakeTrapFrame direct_frame{};
        FakeTrapFrame capability_frame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::capability_call),
            .arg0 = 7u,
            .arg1 = 2u,
            .arg2 = 33u,
        };
        FakeTrapFrame debug_frame{
            .service_id = static_cast<std::uint16_t>(
                kernel::TrapService::debug_write),
            .arg0 = 0xDDu,
        };
        kernel::TaskSyscallFrameView capability_view{};
        const auto captured = direct_adapter.capture(
            direct_adapter.ctx, capability_frame, capability_view);
        const auto applied = direct_adapter.apply_result(
            direct_adapter.ctx,
            direct_frame,
            kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = 9u,
            });
        const auto capability_result = trap_bridge.dispatch(capability_frame);
        const auto debug_result = ingress_bridge.dispatch(debug_frame);

        const auto* first = trap_frame_trace.at(0u);
        const auto* third = trap_frame_trace.at(2u);
        const auto* ingress_second = ingress_frame_trace.at(1u);
        const auto* table_second = table_trace.at(1u);
        if (first == nullptr || third == nullptr || ingress_second == nullptr ||
            table_second == nullptr) {
            return false;
        }

        const auto debug_projection =
            kernel::task_syscall_semantic_projection(*ingress_second);

        return kernel::task_syscall_frame_adapter_ready(direct_adapter) &&
               trap_bridge.valid() && ingress_bridge.valid() && captured &&
               applied &&
               trap_result_matches(capability_result,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               trap_result_matches(debug_result,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0xDDu) &&
               trap_state.capture_calls == 3u &&
               trap_state.writeback_calls == 3u &&
               dispatch_state.capability_calls == 1u &&
               dispatch_state.last_capability_id == 7u &&
               dispatch_state.last_capability_operation == 2u &&
               dispatch_state.last_capability_payload == 33u &&
               debug_state.calls == 1u &&
               debug_state.last_value == 0xDDu &&
               table_trace.size() == 2u &&
               dispatch_trace.size() == 1u &&
               trap_frame_trace.size() == 3u &&
               ingress_frame_trace.size() == 3u &&
               capability_view.syscall ==
                   kernel::TaskSyscallId::capability_call &&
               capability_view.arg0 == 7u && capability_view.arg1 == 2u &&
               capability_view.arg2 == 33u &&
               direct_frame.writeback_seen && direct_frame.return_value == 9u &&
               capability_frame.writeback_seen &&
               capability_frame.return_value == 42u &&
               debug_frame.writeback_seen &&
               debug_frame.return_value == 0xDDu &&
               first->sequence == 1u &&
               first->stage == kernel::TaskSyscallFrameStage::decode &&
               first->syscall == kernel::TaskSyscallId::capability_call &&
               first->ok &&
               third->sequence == 3u &&
               third->stage == kernel::TaskSyscallFrameStage::writeback &&
               third->value == 42u && third->ok &&
               ingress_second->sequence == 2u &&
               ingress_second->stage ==
                   kernel::TaskSyscallFrameStage::dispatch &&
               ingress_second->syscall ==
                   kernel::TaskSyscallId::debug_write &&
               ingress_second->value == 0xDDu && ingress_second->ok &&
               table_second->syscall == kernel::TaskSyscallId::debug_write &&
               table_second->value == 0xDDu &&
               same_text(debug_projection.descriptor.syscall_name,
                         "debug-write"sv) &&
               debug_projection.field_count == 1u &&
               same_text(debug_projection.fields[0].name, "value"sv) &&
               debug_projection.fields[0].value == 0xDDu;
    }

    [[nodiscard]] bool probe_bridge_and_port() noexcept
    {
        FakeDispatchSurfaceState dispatch_state{};
        DirectDebugHandlerState debug_state{};
        FakeFrameAdapterState adapter_state{};
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
                .ctx = &adapter_state,
                .capture = &capture_fake_syscall_frame,
                .apply_result = &apply_fake_syscall_result,
            },
            &frame_trace);
        const FramePort port = kernel::make_task_syscall_frame_port(frame_bridge);

        FakeSyscallFrame yield_frame{
            .syscall_id = static_cast<std::uint16_t>(
                kernel::TaskSyscallId::yield),
        };
        FakeSyscallFrame sleep_frame{
            .syscall_id = static_cast<std::uint16_t>(
                kernel::TaskSyscallId::sleep_until),
            .arg0 = 55u,
        };
        FakeSyscallFrame debug_frame{
            .syscall_id = static_cast<std::uint16_t>(
                kernel::TaskSyscallId::debug_write),
            .arg0 = 0xCCu,
        };
        FakeSyscallFrame capability_frame{
            .syscall_id = static_cast<std::uint16_t>(
                kernel::TaskSyscallId::capability_call),
            .arg0 = 7u,
            .arg1 = 2u,
            .arg2 = 33u,
        };

        const auto yielded = frame_bridge.dispatch(yield_frame);
        const auto slept = frame_bridge.dispatch(sleep_frame);
        const auto debugged = port.dispatch_frame(debug_frame);
        const auto called = frame_bridge.dispatch(capability_frame);

        const auto* first = frame_trace.at(0u);
        const auto* second = frame_trace.at(1u);
        const auto* third = frame_trace.at(2u);
        const auto* eighth = frame_trace.at(7u);
        const auto* ninth = frame_trace.at(8u);
        const auto* twelfth = frame_trace.at(11u);
        const auto* dispatch_first = dispatch_trace.at(0u);
        const auto* table_third = table_trace.at(2u);
        if (first == nullptr || second == nullptr || third == nullptr ||
            eighth == nullptr || ninth == nullptr || twelfth == nullptr ||
            dispatch_first == nullptr || table_third == nullptr) {
            return false;
        }

        const auto debug_projection =
            kernel::task_syscall_semantic_projection(*eighth);

        return frame_bridge.valid() && port.valid() &&
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
               adapter_state.capture_calls == 4u &&
               adapter_state.writeback_calls == 4u &&
               dispatch_trace.size() == 3u &&
               table_trace.size() == 4u &&
               frame_trace.size() == 12u &&
               yield_frame.writeback_seen && yield_frame.return_value == 1u &&
               sleep_frame.writeback_seen && sleep_frame.return_value == 55u &&
               debug_frame.writeback_seen &&
               debug_frame.return_value == 0xCCu &&
               capability_frame.writeback_seen &&
               capability_frame.return_value == 42u &&
               first->sequence == 1u &&
               first->stage == kernel::TaskSyscallFrameStage::decode &&
               same_text(kernel::task_syscall_frame_stage_name(first->stage),
                         "decode"sv) &&
               first->syscall == kernel::TaskSyscallId::yield &&
               first->trap_service == kernel::TrapService::yield_current &&
               first->ok &&
               second->sequence == 2u &&
               second->stage == kernel::TaskSyscallFrameStage::dispatch &&
               second->value == 1u && second->ok &&
               third->sequence == 3u &&
               third->stage == kernel::TaskSyscallFrameStage::writeback &&
               third->value == 1u && third->ok &&
               dispatch_first->syscall == kernel::TaskSyscallId::yield &&
               table_third->syscall == kernel::TaskSyscallId::debug_write &&
               table_third->value == 0xCCu &&
               eighth->sequence == 8u &&
               eighth->stage == kernel::TaskSyscallFrameStage::dispatch &&
               eighth->syscall == kernel::TaskSyscallId::debug_write &&
               eighth->trap_service == kernel::TrapService::debug_write &&
               eighth->value == 0xCCu && eighth->ok &&
               same_text(debug_projection.descriptor.syscall_name,
                         "debug-write"sv) &&
               debug_projection.field_count == 1u &&
               same_text(debug_projection.fields[0].name, "value"sv) &&
               debug_projection.fields[0].value == 0xCCu &&
               ninth->sequence == 9u &&
               ninth->stage == kernel::TaskSyscallFrameStage::writeback &&
               ninth->error == kernel::TrapError::none &&
               ninth->value == 0xCCu && ninth->ok &&
               twelfth->sequence == 12u &&
               twelfth->stage == kernel::TaskSyscallFrameStage::writeback &&
               twelfth->syscall == kernel::TaskSyscallId::capability_call &&
               twelfth->value == 42u && twelfth->ok;
    }

    [[nodiscard]] bool probe_error_paths() noexcept
    {
        const FramePort invalid_port{};
        FakeSyscallFrame port_frame{
            .syscall_id = static_cast<std::uint16_t>(kernel::TaskSyscallId::yield),
        };
        const auto invalid_port_result = invalid_port.dispatch_frame(port_frame);

        {
            TableTrace table_trace{};
            FrameTrace frame_trace{};
            auto table = kernel::make_task_syscall_table(
                std::array<kernel::TaskSyscallHandlerEntry, 1>{
                    kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::yield),
                },
                &table_trace);
            auto unbound_bridge = kernel::make_task_syscall_frame_bridge(
                table,
                kernel::TaskSyscallFrameAdapter<FakeSyscallFrame>{},
                &frame_trace);
            FakeSyscallFrame frame{
                .syscall_id = static_cast<std::uint16_t>(
                    kernel::TaskSyscallId::yield),
            };
            const auto result = unbound_bridge.dispatch(frame);
            const auto* event = frame_trace.at(0u);
            if (!trap_result_matches(invalid_port_result,
                                     kernel::TrapDisposition::rejected,
                                     kernel::TrapError::unbound_adapter) ||
                !trap_result_matches(result,
                                     kernel::TrapDisposition::rejected,
                                     kernel::TrapError::unbound_adapter) ||
                event == nullptr || table_trace.size() != 0u ||
                frame.writeback_seen || event->sequence != 1u ||
                event->stage != kernel::TaskSyscallFrameStage::decode ||
                event->error != kernel::TrapError::unbound_adapter ||
                event->ok) {
                return false;
            }
        }

        {
            FakeFrameAdapterState adapter_state{};
            TableTrace table_trace{};
            FrameTrace frame_trace{};
            auto table = kernel::make_task_syscall_table(
                std::array<kernel::TaskSyscallHandlerEntry, 1>{
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::sleep_until),
                },
                &table_trace);
            auto bridge = kernel::make_task_syscall_frame_bridge(
                table,
                kernel::TaskSyscallFrameAdapter<FakeSyscallFrame>{
                    .ctx = &adapter_state,
                    .capture = &capture_fake_syscall_frame,
                    .apply_result = &apply_fake_syscall_result,
                },
                &frame_trace);
            FakeSyscallFrame frame{
                .syscall_id = static_cast<std::uint16_t>(
                    kernel::TaskSyscallId::sleep_until),
                .arg0 = 21u,
                .capture_enabled = false,
            };

            const auto result = bridge.dispatch(frame);
            const auto* event = frame_trace.at(0u);
            if (!trap_result_matches(result,
                                     kernel::TrapDisposition::rejected,
                                     kernel::TrapError::decode_failed) ||
                event == nullptr || table_trace.size() != 0u ||
                adapter_state.capture_calls != 1u ||
                adapter_state.writeback_calls != 0u ||
                frame.writeback_seen ||
                event->stage != kernel::TaskSyscallFrameStage::decode ||
                event->error != kernel::TrapError::decode_failed ||
                event->ok) {
                return false;
            }
        }

        {
            FakeDispatchSurfaceState dispatch_state{};
            FakeFrameAdapterState adapter_state{};
            DispatchTrace dispatch_trace{};
            TableTrace table_trace{};
            FrameTrace frame_trace{};
            DispatchBridge dispatch_bridge{FakeDispatchSurface{
                                               .state = &dispatch_state,
                                           },
                                           &dispatch_trace};
            auto table = kernel::make_task_syscall_table(
                std::array<kernel::TaskSyscallHandlerEntry, 1>{
                    kernel::task_syscall_handler_entry(
                        kernel::TaskSyscallId::capability_call,
                        kernel::make_task_syscall_handler(dispatch_bridge)),
                },
                &table_trace);
            auto bridge = kernel::make_task_syscall_frame_bridge(
                table,
                kernel::TaskSyscallFrameAdapter<FakeSyscallFrame>{
                    .ctx = &adapter_state,
                    .capture = &capture_fake_syscall_frame,
                    .apply_result = &apply_fake_syscall_result,
                },
                &frame_trace);
            FakeSyscallFrame frame{
                .syscall_id = static_cast<std::uint16_t>(
                    kernel::TaskSyscallId::capability_call),
                .arg0 = 7u,
                .arg1 = 2u,
                .arg2 = 33u,
                .writeback_enabled = false,
            };

            const auto result = bridge.dispatch(frame);
            const auto* dispatch_event = frame_trace.at(1u);
            const auto* writeback_event = frame_trace.at(2u);
            if (!trap_result_matches(result,
                                     kernel::TrapDisposition::rejected,
                                     kernel::TrapError::writeback_failed,
                                     42u) ||
                dispatch_event == nullptr || writeback_event == nullptr ||
                adapter_state.capture_calls != 1u ||
                adapter_state.writeback_calls != 1u ||
                dispatch_state.capability_calls != 1u ||
                dispatch_state.last_capability_id != 7u ||
                dispatch_state.last_capability_operation != 2u ||
                dispatch_state.last_capability_payload != 33u ||
                table_trace.size() != 1u || dispatch_trace.size() != 1u ||
                frame_trace.size() != 3u || frame.writeback_seen ||
                frame.return_value != 0u ||
                dispatch_event->stage !=
                    kernel::TaskSyscallFrameStage::dispatch ||
                dispatch_event->value != 42u || !dispatch_event->ok ||
                writeback_event->stage !=
                    kernel::TaskSyscallFrameStage::writeback ||
                writeback_event->error !=
                    kernel::TrapError::writeback_failed ||
                writeback_event->value != 42u || writeback_event->ok) {
                return false;
            }
        }

        return true;
    }
}

int main()
{
    const bool decode_ok = demo::probe_trap_decode_bridge();
    constexpr bool frame_ok = demo::probe_frame_view_conversion();
    const bool ingress_ok = demo::probe_runtime_trap_adapter_bridge();
    const bool helper_ok = demo::probe_runtime_trap_bridge_helpers();
    const bool bridge_ok = demo::probe_bridge_and_port();
    const bool error_ok = demo::probe_error_paths();
    const bool ok = decode_ok && frame_ok && ingress_ok && helper_ok &&
                    bridge_ok && error_ok;

    std::printf(
        "[runtime-task-syscall-frame-demo] ok=%d decode=%d frame=%d ingress=%d helper=%d bridge=%d error=%d\n",
        ok ? 1 : 0,
        decode_ok ? 1 : 0,
        frame_ok ? 1 : 0,
        ingress_ok ? 1 : 0,
        helper_ok ? 1 : 0,
        bridge_ok ? 1 : 0,
        error_ok ? 1 : 0);
    return ok ? 0 : 1;
}

#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_syscall_dispatch;

namespace demo {
    using namespace std::literals;

    struct FakeDispatchSurfaceState {
        bool bound{true};
        std::uint32_t yield_calls{0};
        std::uint32_t sleep_calls{0};
        std::uint32_t debug_calls{0};
        std::uint32_t capability_calls{0};
        std::uint64_t last_due{0};
        std::uint64_t last_debug_value{0};
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
                return unbound_result();
            }

            ++state->yield_calls;
            return handled_result(1u);
        }

        [[nodiscard]] kernel::TrapResult sleep_current_until(
            kernel::TrapSleepUntilView<tick_type> sleep) const noexcept
        {
            if (!valid()) {
                return unbound_result();
            }

            ++state->sleep_calls;
            state->last_due = sleep.due;
            return handled_result(sleep.due);
        }

        [[nodiscard]] kernel::TrapResult debug_write(
            kernel::TrapDebugWriteView write) const noexcept
        {
            if (!valid()) {
                return unbound_result();
            }

            ++state->debug_calls;
            state->last_debug_value = write.value;
            return handled_result(write.value);
        }

        [[nodiscard]] kernel::TrapResult capability_call(
            kernel::TrapCapabilityCallView capability) const noexcept
        {
            if (!valid()) {
                return unbound_result();
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

        [[nodiscard]] static constexpr kernel::TrapResult unbound_result()
            noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::unbound_bridge,
                .value = 0,
            };
        }
    };

    using DispatchTrace = kernel::TaskSyscallDispatchTraceBuffer<8>;
    using Dispatcher =
        kernel::TaskSyscallDispatcher<FakeDispatchSurface, DispatchTrace>;
    using DispatchPort =
        kernel::TaskSyscallDispatchPort<FakeDispatchSurface::tick_type>;

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

    [[nodiscard]] constexpr bool probe_request_conversion() noexcept
    {
        const auto request = kernel::make_task_syscall_capability_call_request(
            kernel::TrapCapabilityCallView{
                .capability_id = 7u,
                .operation = 2u,
                .payload = 33u,
            });
        const auto trap = kernel::trap_request_from_task_syscall_request(
            request,
            kernel::TrapOrigin::user_task);
        const auto round_trip =
            kernel::task_syscall_request_from_trap_request(trap);
        const auto projection = kernel::task_syscall_semantic_projection(
            request);

        return request.syscall == kernel::TaskSyscallId::capability_call &&
               request.arg0 == 7u && request.arg1 == 2u &&
               request.arg2 == 33u &&
               trap.service == kernel::TrapService::capability_call &&
               trap.origin == kernel::TrapOrigin::user_task &&
               round_trip.syscall == request.syscall &&
               round_trip.arg0 == request.arg0 &&
               round_trip.arg1 == request.arg1 &&
               round_trip.arg2 == request.arg2 &&
               projection.descriptor.syscall ==
                   kernel::TaskSyscallId::capability_call &&
               projection.field_count == 3u &&
               same_text(projection.fields[0].name, "capability-id"sv) &&
               projection.fields[0].value == 7u &&
               same_text(projection.fields[1].name, "operation"sv) &&
               projection.fields[1].value == 2u &&
               same_text(projection.fields[2].name, "payload"sv) &&
               projection.fields[2].value == 33u;
    }

    static_assert(probe_request_conversion());

    [[nodiscard]] bool probe_dispatcher_and_port() noexcept
    {
        FakeDispatchSurfaceState state{};
        DispatchTrace trace{};
        auto dispatcher = kernel::make_task_syscall_dispatcher(
            FakeDispatchSurface{
                .state = &state,
            },
            &trace);
        const DispatchPort port =
            kernel::make_task_syscall_dispatch_port(dispatcher);

        const auto yielded = dispatcher.yield();
        const auto slept = dispatcher.sleep_until(21u);
        const auto debugged = port.debug_write(0x44u);
        const auto called = port.dispatch(kernel::make_capability_call_trap_request(
            kernel::TrapCapabilityCallView{
                .capability_id = 7u,
                .operation = 2u,
                .payload = 33u,
            },
            kernel::TrapOrigin::user_task));

        const auto* first = trace.at(0u);
        const auto* fourth = trace.at(3u);
        if (first == nullptr || fourth == nullptr) {
            return false;
        }

        const auto first_projection =
            kernel::task_syscall_semantic_projection(*first);
        const auto fourth_projection =
            kernel::task_syscall_semantic_projection(*fourth);

        return dispatcher.valid() && port.valid() &&
               trap_result_matches(yielded,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               trap_result_matches(slept,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   21u) &&
               trap_result_matches(debugged,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0x44u) &&
               trap_result_matches(called,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               state.yield_calls == 1u && state.sleep_calls == 1u &&
               state.debug_calls == 1u && state.capability_calls == 1u &&
               state.last_due == 21u && state.last_debug_value == 0x44u &&
               state.last_capability_id == 7u &&
               state.last_capability_operation == 2u &&
               state.last_capability_payload == 33u &&
               trace.size() == 4u &&
               first->sequence == 1u &&
               first->syscall == kernel::TaskSyscallId::yield &&
               first->trap_service == kernel::TrapService::yield_current &&
               same_text(first_projection.descriptor.syscall_name,
                         "yield"sv) &&
               first_projection.field_count == 0u &&
               fourth->sequence == 4u &&
               fourth->syscall ==
                   kernel::TaskSyscallId::capability_call &&
               same_text(fourth_projection.descriptor.syscall_name,
                         "capability-call"sv) &&
               fourth_projection.field_count == 3u &&
               same_text(fourth_projection.fields[0].name,
                         "capability-id"sv) &&
               fourth_projection.fields[0].value == 7u &&
               same_text(fourth_projection.fields[1].name, "operation"sv) &&
               fourth_projection.fields[1].value == 2u &&
               same_text(fourth_projection.fields[2].name, "payload"sv) &&
               fourth_projection.fields[2].value == 33u;
    }

    [[nodiscard]] bool probe_unbound_and_unsupported() noexcept
    {
        DispatchTrace trace{};
        Dispatcher dispatcher{FakeDispatchSurface{}, &trace};
        const DispatchPort unbound_port{};

        const auto unbound = dispatcher.yield();
        const auto unbound_from_port = unbound_port.yield();
        FakeDispatchSurfaceState rebound_state{};
        dispatcher.bind_surface(FakeDispatchSurface{
            .state = &rebound_state,
        });
        const auto unsupported = dispatcher.dispatch(kernel::TaskSyscallRequest{
            .syscall = static_cast<kernel::TaskSyscallId>(99u),
            .arg0 = 0xAAu,
            .arg1 = 0xBBu,
        });

        const auto* first = trace.at(0u);
        const auto* second = trace.at(1u);
        if (first == nullptr || second == nullptr) {
            return false;
        }

        const auto second_projection =
            kernel::task_syscall_semantic_projection(*second);

        return !Dispatcher{}.valid() &&
               trap_result_matches(unbound,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               trap_result_matches(unbound_from_port,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               trap_result_matches(unsupported,
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service) &&
               trace.size() == 2u &&
               first->sequence == 1u &&
               first->error == kernel::TrapError::unbound_bridge &&
               second->sequence == 2u &&
               second->syscall ==
                   static_cast<kernel::TaskSyscallId>(99u) &&
               second->trap_service == kernel::TrapService::invalid &&
               second->error == kernel::TrapError::unsupported_service &&
               same_text(second_projection.descriptor.syscall_name,
                         "unknown"sv) &&
               second_projection.field_count == 4u &&
               same_text(second_projection.fields[0].name, "arg0"sv) &&
               second_projection.fields[0].value == 0xAAu &&
               same_text(second_projection.fields[1].name, "arg1"sv) &&
               second_projection.fields[1].value == 0xBBu &&
               rebound_state.yield_calls == 0u &&
               rebound_state.capability_calls == 0u;
    }
}

int main()
{
    constexpr bool request_ok = demo::probe_request_conversion();
    const bool dispatcher_ok = demo::probe_dispatcher_and_port();
    const bool error_ok = demo::probe_unbound_and_unsupported();
    const bool ok = request_ok && dispatcher_ok && error_ok;

    std::printf(
        "[runtime-task-syscall-dispatch-demo] ok=%d request=%d dispatcher=%d error=%d\n",
        ok ? 1 : 0,
        request_ok ? 1 : 0,
        dispatcher_ok ? 1 : 0,
        error_ok ? 1 : 0);
    return ok ? 0 : 1;
}

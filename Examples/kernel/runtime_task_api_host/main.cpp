#include <cstdint>
#include <cstdio>

import kernel.task_runtime_api;

namespace demo {
    struct FakeTrapTransportState {
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

    struct FakeTrapTransport {
        using tick_type = std::uint64_t;

        FakeTrapTransportState* state{nullptr};

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
                                  capability.operation + capability.payload);
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

    using RuntimeServices =
        kernel::RuntimeTrapServiceFacade<FakeTrapTransport>;
    using TaskRuntime = kernel::TaskRuntimeApi<RuntimeServices>;

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    [[nodiscard]] bool probe_default_unbound_task_runtime() noexcept
    {
        TaskRuntime runtime{};
        return !runtime.valid() && !runtime.services().valid() &&
               trap_result_matches(runtime.yield(),
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               trap_result_matches(runtime.sleep_until(7u),
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               trap_result_matches(runtime.debug_write(0x33u),
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               trap_result_matches(runtime.capability_call(1u, 2u, 3u),
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge);
    }

    [[nodiscard]] bool probe_task_named_entry_points() noexcept
    {
        FakeTrapTransportState state{};
        auto runtime = kernel::make_task_runtime_api(
            kernel::make_runtime_trap_service_facade(FakeTrapTransport{
                .state = &state,
            }));

        const auto yielded = runtime.yield();
        const auto yielded_view =
            runtime.yield(kernel::TrapYieldCurrentView{});
        const auto slept = runtime.sleep_until(42u);
        const auto slept_view = runtime.sleep_until(
            kernel::TrapSleepUntilView<FakeTrapTransport::tick_type>{
                .due = 84u,
            });
        const auto debugged = runtime.debug_write(0xC0DEu);
        const auto debugged_view = runtime.debug_write(
            kernel::TrapDebugWriteView{
                .value = 0x55u,
            });
        const auto called = runtime.capability_call(7u, 2u, 33u);
        const auto called_view = runtime.capability_call(
            kernel::TrapCapabilityCallView{
                .capability_id = 9u,
                .operation = 4u,
                .payload = 5u,
            });

        return runtime.valid() && runtime.services().valid() &&
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
                                   42u) &&
               trap_result_matches(slept_view,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   84u) &&
               trap_result_matches(debugged,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0xC0DEu) &&
               trap_result_matches(debugged_view,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0x55u) &&
               trap_result_matches(called,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               trap_result_matches(called_view,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   18u) &&
               state.yield_calls == 2u && state.sleep_calls == 2u &&
               state.debug_calls == 2u && state.capability_calls == 2u &&
               state.last_due == 84u && state.last_debug_value == 0x55u &&
               state.last_capability_id == 9u &&
               state.last_capability_operation == 4u &&
               state.last_capability_payload == 5u;
    }

    [[nodiscard]] bool probe_bind_services() noexcept
    {
        FakeTrapTransportState first{};
        FakeTrapTransportState second{};
        TaskRuntime runtime{kernel::make_runtime_trap_service_facade(
            FakeTrapTransport{
                .state = &first,
            })};

        const auto first_yield = runtime.yield();
        runtime.bind_services(
            kernel::make_runtime_trap_service_facade(FakeTrapTransport{}));
        const bool unbound_valid = runtime.valid();
        const auto unbound_sleep = runtime.sleep_until(9u);
        runtime.bind_services(kernel::make_runtime_trap_service_facade(
            FakeTrapTransport{
                .state = &second,
            }));
        const auto rebound_capability = runtime.capability_call(5u, 6u);

        return trap_result_matches(first_yield,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               !unbound_valid &&
               trap_result_matches(unbound_sleep,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               runtime.valid() && runtime.services().valid() &&
               runtime.services().transport().state == &second &&
               trap_result_matches(rebound_capability,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   11u) &&
               first.yield_calls == 1u && second.capability_calls == 1u &&
               second.last_capability_id == 5u &&
               second.last_capability_operation == 6u &&
               second.last_capability_payload == 0u;
    }
}

int main()
{
    const bool default_unbound_ok = demo::probe_default_unbound_task_runtime();
    const bool entry_points_ok = demo::probe_task_named_entry_points();
    const bool bind_ok = demo::probe_bind_services();
    const bool ok = default_unbound_ok && entry_points_ok && bind_ok;

    std::printf(
        "[runtime-task-api-demo] ok=%d default_unbound=%d entry_points=%d bind=%d\n",
        ok ? 1 : 0,
        default_unbound_ok ? 1 : 0,
        entry_points_ok ? 1 : 0,
        bind_ok ? 1 : 0);
    return ok ? 0 : 1;
}

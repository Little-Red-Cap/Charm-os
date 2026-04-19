#include <cstdint>
#include <cstdio>

import kernel.task_syscall_api;

namespace demo {
    struct FakeTrapTransportState {
        bool bound{true};
        std::uint64_t token{0};
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
            return handled_result(state->token + 1u);
        }

        [[nodiscard]] kernel::TrapResult sleep_current_until(
            kernel::TrapSleepUntilView<tick_type> sleep) const noexcept
        {
            if (!valid()) {
                return unbound_result();
            }

            ++state->sleep_calls;
            state->last_due = sleep.due;
            return handled_result(state->token + sleep.due);
        }

        [[nodiscard]] kernel::TrapResult debug_write(
            kernel::TrapDebugWriteView write) const noexcept
        {
            if (!valid()) {
                return unbound_result();
            }

            ++state->debug_calls;
            state->last_debug_value = write.value;
            return handled_result(state->token + write.value);
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
            return handled_result(state->token + capability.capability_id +
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
    using TaskSyscalls = kernel::TaskSyscallApi<TaskRuntime>;

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    [[nodiscard]] bool probe_binding_chain() noexcept
    {
        FakeTrapTransportState first{.token = 1000u};
        FakeTrapTransportState second{.token = 2000u};
        FakeTrapTransportState third{.token = 3000u};
        FakeTrapTransportState fourth{.token = 4000u};

        auto syscalls = kernel::make_task_syscall_api(
            kernel::make_task_runtime_api(
                kernel::make_runtime_trap_service_facade(FakeTrapTransport{
                    .state = &first,
                })));

        const bool initial_getters_ok =
            syscalls.valid() && syscalls.runtime().valid() &&
            syscalls.runtime().services().valid() &&
            syscalls.runtime().services().transport().state == &first;

        const auto first_debug = syscalls.sys_debug_write(10u);

        syscalls.runtime().services().bind_transport(FakeTrapTransport{});
        const bool lower_unbound_ok =
            !syscalls.valid() && !syscalls.runtime().valid() &&
            !syscalls.runtime().services().valid() &&
            trap_result_matches(syscalls.sys_yield(),
                                kernel::TrapDisposition::rejected,
                                kernel::TrapError::unbound_bridge);

        syscalls.runtime().services().bind_transport(FakeTrapTransport{
            .state = &second,
        });
        const bool lower_rebound_getters_ok =
            syscalls.valid() && syscalls.runtime().valid() &&
            syscalls.runtime().services().valid() &&
            syscalls.runtime().services().transport().state == &second;
        const auto second_capability = syscalls.sys_capability_call(5u, 6u);

        syscalls.runtime().bind_services(
            kernel::make_runtime_trap_service_facade(FakeTrapTransport{}));
        const bool middle_unbound_ok =
            !syscalls.valid() && !syscalls.runtime().valid() &&
            !syscalls.runtime().services().valid() &&
            trap_result_matches(syscalls.sys_sleep_until(9u),
                                kernel::TrapDisposition::rejected,
                                kernel::TrapError::unbound_bridge);

        syscalls.runtime().bind_services(
            kernel::make_runtime_trap_service_facade(FakeTrapTransport{
                .state = &third,
            }));
        const bool middle_rebound_getters_ok =
            syscalls.valid() && syscalls.runtime().valid() &&
            syscalls.runtime().services().valid() &&
            syscalls.runtime().services().transport().state == &third;
        const auto third_sleep = syscalls.sys_sleep_until(42u);

        syscalls.bind_runtime(kernel::make_task_runtime_api(
            kernel::make_runtime_trap_service_facade(FakeTrapTransport{})));
        const bool top_unbound_ok =
            !syscalls.valid() && !syscalls.runtime().valid() &&
            !syscalls.runtime().services().valid() &&
            trap_result_matches(syscalls.sys_debug_write(0x55u),
                                kernel::TrapDisposition::rejected,
                                kernel::TrapError::unbound_bridge);

        syscalls.bind_runtime(kernel::make_task_runtime_api(
            kernel::make_runtime_trap_service_facade(FakeTrapTransport{
                .state = &fourth,
            })));
        const bool top_rebound_getters_ok =
            syscalls.valid() && syscalls.runtime().valid() &&
            syscalls.runtime().services().valid() &&
            syscalls.runtime().services().transport().state == &fourth;
        const auto fourth_yield = syscalls.sys_yield();

        const bool result_ok =
            trap_result_matches(first_debug,
                                kernel::TrapDisposition::handled,
                                kernel::TrapError::none,
                                1010u) &&
            trap_result_matches(second_capability,
                                kernel::TrapDisposition::handled,
                                kernel::TrapError::none,
                                2011u) &&
            trap_result_matches(third_sleep,
                                kernel::TrapDisposition::handled,
                                kernel::TrapError::none,
                                3042u) &&
            trap_result_matches(fourth_yield,
                                kernel::TrapDisposition::handled,
                                kernel::TrapError::none,
                                4001u);

        const bool state_ok =
            first.debug_calls == 1u && first.last_debug_value == 10u &&
            second.capability_calls == 1u &&
            second.last_capability_id == 5u &&
            second.last_capability_operation == 6u &&
            second.last_capability_payload == 0u &&
            third.sleep_calls == 1u && third.last_due == 42u &&
            fourth.yield_calls == 1u;

        const bool ok = initial_getters_ok && lower_unbound_ok &&
                        lower_rebound_getters_ok && middle_unbound_ok &&
                        middle_rebound_getters_ok && top_unbound_ok &&
                        top_rebound_getters_ok && result_ok && state_ok;

        std::printf(
            "[runtime-binding-chain-demo] ok=%d initial=%d lower_unbound=%d lower_rebind=%d middle_unbound=%d middle_rebind=%d top_unbound=%d top_rebind=%d results=%d state=%d\n",
            ok ? 1 : 0,
            initial_getters_ok ? 1 : 0,
            lower_unbound_ok ? 1 : 0,
            lower_rebound_getters_ok ? 1 : 0,
            middle_unbound_ok ? 1 : 0,
            middle_rebound_getters_ok ? 1 : 0,
            top_unbound_ok ? 1 : 0,
            top_rebound_getters_ok ? 1 : 0,
            result_ok ? 1 : 0,
            state_ok ? 1 : 0);
        std::printf(
            "[runtime-binding-chain-values] debug=%llu capability=%llu sleep=%llu yield=%llu\n",
            static_cast<unsigned long long>(first_debug.value),
            static_cast<unsigned long long>(second_capability.value),
            static_cast<unsigned long long>(third_sleep.value),
            static_cast<unsigned long long>(fourth_yield.value));

        return ok;
    }
}

int main()
{
    const bool ok = demo::probe_binding_chain();
    return ok ? 0 : 1;
}

#include <cstdint>
#include <cstdio>

import kernel.runtime_service;

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

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    [[nodiscard]] bool probe_default_unbound_facade() noexcept
    {
        kernel::RuntimeTrapServiceFacade<FakeTrapTransport> facade{};
        return !facade.valid() && !facade.transport().valid() &&
               trap_result_matches(facade.yield_current(),
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               trap_result_matches(facade.sleep_current_until(7u),
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               trap_result_matches(facade.debug_write(0x33u),
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               trap_result_matches(facade.capability_call(1u, 2u, 3u),
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge);
    }

    [[nodiscard]] bool probe_service_overloads() noexcept
    {
        FakeTrapTransportState state{};
        auto facade = kernel::make_runtime_trap_service_facade(
            FakeTrapTransport{
                .state = &state,
            });

        const auto yielded = facade.yield_current();
        const auto yielded_view =
            facade.yield_current(kernel::TrapYieldCurrentView{});
        const auto slept_scalar = facade.sleep_current_until(42u);
        const auto slept_view =
            facade.sleep_current_until(kernel::TrapSleepUntilView<
                                       FakeTrapTransport::tick_type>{
                .due = 84u,
            });
        const auto debugged_scalar = facade.debug_write(0xC0DEu);
        const auto debugged_view = facade.debug_write(
            kernel::TrapDebugWriteView{
                .value = 0x55u,
            });
        const auto called_scalar = facade.capability_call(7u, 2u, 33u);
        const auto called_view = facade.capability_call(
            kernel::TrapCapabilityCallView{
                .capability_id = 9u,
                .operation = 4u,
                .payload = 5u,
            });

        return facade.valid() && facade.transport().state == &state &&
               trap_result_matches(yielded,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               trap_result_matches(yielded_view,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               trap_result_matches(slept_scalar,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               trap_result_matches(slept_view,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   84u) &&
               trap_result_matches(debugged_scalar,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0xC0DEu) &&
               trap_result_matches(debugged_view,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0x55u) &&
               trap_result_matches(called_scalar,
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

    [[nodiscard]] bool probe_bind_transport_and_default_payload() noexcept
    {
        FakeTrapTransportState first{};
        FakeTrapTransportState second{};
        kernel::RuntimeTrapServiceFacade<FakeTrapTransport> facade{
            FakeTrapTransport{
                .state = &first,
            },
        };

        const auto first_debug = facade.debug_write(0x10u);
        facade.bind_transport(FakeTrapTransport{});
        const bool unbound_valid = facade.valid();
        const auto unbound_debug = facade.debug_write(0x20u);
        facade.bind_transport(FakeTrapTransport{
            .state = &second,
        });
        const auto rebound_capability = facade.capability_call(5u, 6u);

        return trap_result_matches(first_debug,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0x10u) &&
               !unbound_valid &&
               trap_result_matches(unbound_debug,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_bridge) &&
               facade.valid() && facade.transport().state == &second &&
               trap_result_matches(rebound_capability,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   11u) &&
               first.debug_calls == 1u &&
               first.last_debug_value == 0x10u &&
               second.capability_calls == 1u &&
               second.last_capability_id == 5u &&
               second.last_capability_operation == 6u &&
               second.last_capability_payload == 0u;
    }
}

int main()
{
    const bool default_unbound_ok = demo::probe_default_unbound_facade();
    const bool overloads_ok = demo::probe_service_overloads();
    const bool bind_ok = demo::probe_bind_transport_and_default_payload();
    const bool ok = default_unbound_ok && overloads_ok && bind_ok;

    std::printf("[runtime-service-demo] ok=%d default_unbound=%d overloads=%d bind=%d\n",
                ok ? 1 : 0,
                default_unbound_ok ? 1 : 0,
                overloads_ok ? 1 : 0,
                bind_ok ? 1 : 0);
    return ok ? 0 : 1;
}

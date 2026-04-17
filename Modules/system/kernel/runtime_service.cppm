export module kernel.runtime_service;

export import kernel.runtime_trap;
import util.core;

export namespace kernel {
    template <typename Transport>
    class RuntimeTrapServiceFacade {
    public:
        using transport_type = Transport;
        using tick_type = typename Transport::tick_type;

        constexpr RuntimeTrapServiceFacade() noexcept = default;

        constexpr explicit RuntimeTrapServiceFacade(
            Transport transport) noexcept
            : transport_(transport)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return transport_.valid();
        }

        [[nodiscard]] Transport& transport() noexcept
        {
            return transport_;
        }

        [[nodiscard]] const Transport& transport() const noexcept
        {
            return transport_;
        }

        void bind_transport(Transport transport) noexcept
        {
            transport_ = transport;
        }

        [[nodiscard]] TrapResult yield_current() const noexcept
        {
            return yield_current(TrapYieldCurrentView{});
        }

        [[nodiscard]] TrapResult yield_current(
            TrapYieldCurrentView yield) const noexcept
        {
            return transport_.yield_current(yield);
        }

        [[nodiscard]] TrapResult sleep_current_until(
            tick_type due) const noexcept
        {
            return sleep_current_until(TrapSleepUntilView<tick_type>{
                .due = due,
            });
        }

        [[nodiscard]] TrapResult sleep_current_until(
            TrapSleepUntilView<tick_type> sleep) const noexcept
        {
            return transport_.sleep_current_until(sleep);
        }

        [[nodiscard]] TrapResult debug_write(util::u64 value) const noexcept
        {
            return debug_write(TrapDebugWriteView{
                .value = value,
            });
        }

        [[nodiscard]] TrapResult debug_write(
            TrapDebugWriteView write) const noexcept
        {
            return transport_.debug_write(write);
        }

        [[nodiscard]] TrapResult capability_call(
            util::u64 capability_id,
            util::u64 operation,
            util::u64 payload = 0) const noexcept
        {
            return capability_call(TrapCapabilityCallView{
                .capability_id = capability_id,
                .operation = operation,
                .payload = payload,
            });
        }

        [[nodiscard]] TrapResult capability_call(
            TrapCapabilityCallView capability) const noexcept
        {
            return transport_.capability_call(capability);
        }

    private:
        Transport transport_{};
    };

    template <typename Transport>
    [[nodiscard]] auto make_runtime_trap_service_facade(
        Transport transport) noexcept -> RuntimeTrapServiceFacade<Transport>
    {
        return RuntimeTrapServiceFacade<Transport>{transport};
    }
}

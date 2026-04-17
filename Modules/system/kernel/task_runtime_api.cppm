export module kernel.task_runtime_api;

export import kernel.runtime_service;
import util.core;

export namespace kernel {
    template <typename Services>
    class TaskRuntimeApi {
    public:
        using services_type = Services;
        using tick_type = typename Services::tick_type;

        constexpr TaskRuntimeApi() noexcept = default;

        constexpr explicit TaskRuntimeApi(Services services) noexcept
            : services_(services)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return services_.valid();
        }

        [[nodiscard]] Services& services() noexcept
        {
            return services_;
        }

        [[nodiscard]] const Services& services() const noexcept
        {
            return services_;
        }

        void bind_services(Services services) noexcept
        {
            services_ = services;
        }

        [[nodiscard]] TrapResult yield() const noexcept
        {
            return yield(TrapYieldCurrentView{});
        }

        [[nodiscard]] TrapResult yield(TrapYieldCurrentView yield_view) const
            noexcept
        {
            return services_.yield_current(yield_view);
        }

        [[nodiscard]] TrapResult sleep_until(tick_type due) const noexcept
        {
            return sleep_until(TrapSleepUntilView<tick_type>{
                .due = due,
            });
        }

        [[nodiscard]] TrapResult sleep_until(
            TrapSleepUntilView<tick_type> sleep) const noexcept
        {
            return services_.sleep_current_until(sleep);
        }

        [[nodiscard]] TrapResult debug_write(util::u64 value) const noexcept
        {
            return services_.debug_write(value);
        }

        [[nodiscard]] TrapResult debug_write(
            TrapDebugWriteView write) const noexcept
        {
            return services_.debug_write(write);
        }

        [[nodiscard]] TrapResult capability_call(
            util::u64 capability_id,
            util::u64 operation,
            util::u64 payload = 0) const noexcept
        {
            return services_.capability_call(
                capability_id, operation, payload);
        }

        [[nodiscard]] TrapResult capability_call(
            TrapCapabilityCallView capability) const noexcept
        {
            return services_.capability_call(capability);
        }

    private:
        Services services_{};
    };

    template <typename Services>
    [[nodiscard]] auto make_task_runtime_api(
        Services services) noexcept -> TaskRuntimeApi<Services>
    {
        return TaskRuntimeApi<Services>{services};
    }
}

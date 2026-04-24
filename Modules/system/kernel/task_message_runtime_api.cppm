module;

#include <cstddef>

export module kernel.task_message_runtime_api;

export import kernel.task_message_runtime_service;
import util.core;

export namespace kernel {
    template <typename Services>
    class TaskMessageRuntimeApi {
    public:
        using services_type = Services;
        using tick_type = typename Services::tick_type;
        using completion_type = typename Services::completion_type;
        using result_type = typename Services::result_type;

        constexpr TaskMessageRuntimeApi() noexcept = default;

        constexpr explicit TaskMessageRuntimeApi(Services services) noexcept
            : services_(services)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return services_.valid();
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return services_.busy();
        }

        [[nodiscard]] std::size_t pending_requests() const noexcept
        {
            return services_.pending_requests();
        }

        [[nodiscard]] std::size_t pending_completions() const noexcept
        {
            return services_.pending_completions();
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

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            services_.bind_cursors(next_token, next_sequence);
        }

        [[nodiscard]] bool yield(tick_type wait_due) noexcept
        {
            return yield(TrapYieldCurrentView{}, wait_due);
        }

        [[nodiscard]] bool yield(TrapYieldCurrentView yield_view,
                                 tick_type wait_due) noexcept
        {
            return services_.yield_current(yield_view, wait_due);
        }

        [[nodiscard]] bool sleep_until(tick_type due,
                                       tick_type wait_due) noexcept
        {
            return sleep_until(TrapSleepUntilView<tick_type>{
                                   .due = due,
                               },
                               wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sleep_until(TrapSleepUntilView<Tick> sleep,
                                       tick_type wait_due) noexcept
        {
            return services_.sleep_current_until(sleep, wait_due);
        }

        [[nodiscard]] bool debug_write(util::u64 value,
                                       tick_type wait_due) noexcept
        {
            return debug_write(TrapDebugWriteView{
                                   .value = value,
                               },
                               wait_due);
        }

        [[nodiscard]] bool debug_write(TrapDebugWriteView write,
                                       tick_type wait_due) noexcept
        {
            return services_.debug_write(write, wait_due);
        }

        [[nodiscard]] bool capability_call(util::u64 capability_id,
                                           util::u64 operation,
                                           util::u64 payload,
                                           tick_type wait_due) noexcept
        {
            return capability_call(TrapCapabilityCallView{
                                       .capability_id = capability_id,
                                       .operation = operation,
                                       .payload = payload,
                                   },
                                   wait_due);
        }

        [[nodiscard]] bool capability_call(TrapCapabilityCallView capability,
                                           tick_type wait_due) noexcept
        {
            return services_.capability_call(capability, wait_due);
        }

        [[nodiscard]] bool kick() noexcept
        {
            return services_.kick();
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            return services_.step(event);
        }

        [[nodiscard]] bool receive_completion(completion_type& out) noexcept
        {
            return services_.receive_completion(out);
        }

    private:
        Services services_{};
    };

    template <typename Services>
    [[nodiscard]] auto make_task_message_runtime_api(
        Services services) noexcept -> TaskMessageRuntimeApi<Services>
    {
        return TaskMessageRuntimeApi<Services>{services};
    }
}

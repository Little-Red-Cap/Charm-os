module;

#include <cstddef>

export module kernel.task_message_syscall_api;

export import kernel.task_message_runtime_api;
import util.core;

export namespace kernel {
    template <typename Runtime>
    class TaskMessageSyscallApi {
    public:
        using runtime_type = Runtime;
        using tick_type = typename Runtime::tick_type;
        using completion_type = typename Runtime::completion_type;
        using result_type = typename Runtime::result_type;

        constexpr TaskMessageSyscallApi() noexcept = default;

        constexpr explicit TaskMessageSyscallApi(Runtime runtime) noexcept
            : runtime_(runtime)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return runtime_.valid();
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return runtime_.busy();
        }

        [[nodiscard]] std::size_t pending_requests() const noexcept
        {
            return runtime_.pending_requests();
        }

        [[nodiscard]] std::size_t pending_completions() const noexcept
        {
            return runtime_.pending_completions();
        }

        [[nodiscard]] Runtime& runtime() noexcept
        {
            return runtime_;
        }

        [[nodiscard]] const Runtime& runtime() const noexcept
        {
            return runtime_;
        }

        void bind_runtime(Runtime runtime) noexcept
        {
            runtime_ = runtime;
        }

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            runtime_.bind_cursors(next_token, next_sequence);
        }

        [[nodiscard]] bool sys_yield(tick_type wait_due) noexcept
        {
            return sys_yield(TrapYieldCurrentView{}, wait_due);
        }

        [[nodiscard]] bool sys_yield(TrapYieldCurrentView yield_view,
                                     tick_type wait_due) noexcept
        {
            return runtime_.yield(yield_view, wait_due);
        }

        [[nodiscard]] bool sys_sleep_until(tick_type due,
                                           tick_type wait_due) noexcept
        {
            return sys_sleep_until(TrapSleepUntilView<tick_type>{
                                       .due = due,
                                   },
                                   wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sys_sleep_until(TrapSleepUntilView<Tick> sleep,
                                           tick_type wait_due) noexcept
        {
            return runtime_.sleep_until(sleep, wait_due);
        }

        [[nodiscard]] bool sys_debug_write(util::u64 value,
                                           tick_type wait_due) noexcept
        {
            return sys_debug_write(TrapDebugWriteView{
                                       .value = value,
                                   },
                                   wait_due);
        }

        [[nodiscard]] bool sys_debug_write(TrapDebugWriteView write,
                                           tick_type wait_due) noexcept
        {
            return runtime_.debug_write(write, wait_due);
        }

        [[nodiscard]] bool sys_capability_call(util::u64 capability_id,
                                               util::u64 operation,
                                               tick_type wait_due) noexcept
        {
            return sys_capability_call(
                capability_id, operation, 0u, wait_due);
        }

        [[nodiscard]] bool sys_capability_call(util::u64 capability_id,
                                               util::u64 operation,
                                               util::u64 payload,
                                               tick_type wait_due) noexcept
        {
            return sys_capability_call(TrapCapabilityCallView{
                                           .capability_id = capability_id,
                                           .operation = operation,
                                           .payload = payload,
                                       },
                                       wait_due);
        }

        [[nodiscard]] bool sys_capability_call(
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return runtime_.capability_call(capability, wait_due);
        }

        [[nodiscard]] bool kick() noexcept
        {
            return runtime_.kick();
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            return runtime_.step(event);
        }

        [[nodiscard]] bool receive_completion(completion_type& out) noexcept
        {
            return runtime_.receive_completion(out);
        }

    private:
        Runtime runtime_{};
    };

    template <typename Runtime>
    [[nodiscard]] auto make_task_message_syscall_api(
        Runtime runtime) noexcept -> TaskMessageSyscallApi<Runtime>
    {
        return TaskMessageSyscallApi<Runtime>{runtime};
    }
}

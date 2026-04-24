module;

#include <cstddef>

export module kernel.task_message_runtime_service;

export import kernel.runtime_service;
export import kernel.task_message_syscall_pump;
import util.core;

export namespace kernel {
    template <typename Pump>
    class TaskMessageRuntimeServiceFacade {
    public:
        using pump_type = Pump;
        using tick_type = typename Pump::tick_type;
        using reply_type = typename Pump::reply_type;
        using request_type = typename Pump::request_type;
        using completion_type = typename Pump::completion_type;
        using result_type = typename Pump::result_type;

        constexpr TaskMessageRuntimeServiceFacade() noexcept = default;

        constexpr explicit TaskMessageRuntimeServiceFacade(Pump pump) noexcept
            : pump_(pump)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return pump_.valid();
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return pump_.busy();
        }

        [[nodiscard]] std::size_t pending_requests() const noexcept
        {
            return pump_.pending_requests();
        }

        [[nodiscard]] std::size_t pending_completions() const noexcept
        {
            return pump_.pending_completions();
        }

        [[nodiscard]] Pump& pump() noexcept
        {
            return pump_;
        }

        [[nodiscard]] const Pump& pump() const noexcept
        {
            return pump_;
        }

        void bind_pump(Pump pump) noexcept
        {
            pump_ = pump;
        }

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            pump_.bind_cursors(next_token, next_sequence);
        }

        [[nodiscard]] bool enqueue(request_type request) noexcept
        {
            return pump_.enqueue(request);
        }

        [[nodiscard]] bool enqueue(TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return pump_.enqueue(request, wait_due);
        }

        [[nodiscard]] bool enqueue(TaskId owner,
                                   TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return pump_.enqueue(owner, request, wait_due);
        }

        [[nodiscard]] bool enqueue(TaskId owner,
                                   util::u64 token,
                                   util::u64 request_sequence,
                                   TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return pump_.enqueue(
                owner, token, request_sequence, request, wait_due);
        }

        [[nodiscard]] bool yield_current(tick_type wait_due) noexcept
        {
            return yield_current(TrapYieldCurrentView{}, wait_due);
        }

        [[nodiscard]] bool yield_current(TrapYieldCurrentView,
                                         tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_yield_request(), wait_due);
        }

        [[nodiscard]] bool yield_current(TaskId owner,
                                         tick_type wait_due) noexcept
        {
            return yield_current(owner, TrapYieldCurrentView{}, wait_due);
        }

        [[nodiscard]] bool yield_current(TaskId owner,
                                         TrapYieldCurrentView,
                                         tick_type wait_due) noexcept
        {
            return enqueue(owner, make_task_syscall_yield_request(), wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sleep_current_until(
            TrapSleepUntilView<Tick> sleep,
            tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_sleep_until_request(sleep),
                           wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sleep_current_until(
            TaskId owner,
            TrapSleepUntilView<Tick> sleep,
            tick_type wait_due) noexcept
        {
            return enqueue(owner,
                           make_task_syscall_sleep_until_request(sleep),
                           wait_due);
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
            return enqueue(make_task_syscall_debug_write_request(write),
                           wait_due);
        }

        [[nodiscard]] bool debug_write(TaskId owner,
                                       util::u64 value,
                                       tick_type wait_due) noexcept
        {
            return debug_write(owner,
                               TrapDebugWriteView{
                                   .value = value,
                               },
                               wait_due);
        }

        [[nodiscard]] bool debug_write(TaskId owner,
                                       TrapDebugWriteView write,
                                       tick_type wait_due) noexcept
        {
            return enqueue(owner,
                           make_task_syscall_debug_write_request(write),
                           wait_due);
        }

        [[nodiscard]] bool capability_call(
            util::u64 capability_id,
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

        [[nodiscard]] bool capability_call(
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_capability_call_request(capability),
                           wait_due);
        }

        [[nodiscard]] bool capability_call(
            TaskId owner,
            util::u64 capability_id,
            util::u64 operation,
            util::u64 payload,
            tick_type wait_due) noexcept
        {
            return capability_call(owner,
                                   TrapCapabilityCallView{
                                       .capability_id = capability_id,
                                       .operation = operation,
                                       .payload = payload,
                                   },
                                   wait_due);
        }

        [[nodiscard]] bool capability_call(
            TaskId owner,
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return enqueue(owner,
                           make_task_syscall_capability_call_request(capability),
                           wait_due);
        }

        [[nodiscard]] bool kick() noexcept
        {
            return pump_.kick();
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            return pump_.step(event);
        }

        [[nodiscard]] bool receive_completion(completion_type& out) noexcept
        {
            return pump_.receive_completion(out);
        }

    private:
        Pump pump_{};
    };

    template <typename Pump>
    [[nodiscard]] auto make_task_message_runtime_service_facade(
        Pump pump) noexcept -> TaskMessageRuntimeServiceFacade<Pump>
    {
        return TaskMessageRuntimeServiceFacade<Pump>{pump};
    }
}

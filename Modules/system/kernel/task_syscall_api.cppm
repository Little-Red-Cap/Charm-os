export module kernel.task_syscall_api;

export import kernel.task_runtime_api;
import util.core;

export namespace kernel {
    template <typename Runtime>
    class TaskSyscallApi {
    public:
        using runtime_type = Runtime;
        using tick_type = typename Runtime::tick_type;

        constexpr TaskSyscallApi() noexcept = default;

        constexpr explicit TaskSyscallApi(Runtime runtime) noexcept
            : runtime_(runtime)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return runtime_.valid();
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

        [[nodiscard]] TrapResult sys_yield() const noexcept
        {
            return sys_yield(TrapYieldCurrentView{});
        }

        [[nodiscard]] TrapResult sys_yield(
            TrapYieldCurrentView yield_view) const noexcept
        {
            return runtime_.yield(yield_view);
        }

        [[nodiscard]] TrapResult sys_sleep_until(
            tick_type due) const noexcept
        {
            return sys_sleep_until(TrapSleepUntilView<tick_type>{
                .due = due,
            });
        }

        [[nodiscard]] TrapResult sys_sleep_until(
            TrapSleepUntilView<tick_type> sleep) const noexcept
        {
            return runtime_.sleep_until(sleep);
        }

        [[nodiscard]] TrapResult sys_debug_write(
            util::u64 value) const noexcept
        {
            return runtime_.debug_write(value);
        }

        [[nodiscard]] TrapResult sys_debug_write(
            TrapDebugWriteView write) const noexcept
        {
            return runtime_.debug_write(write);
        }

        [[nodiscard]] TrapResult sys_capability_call(
            util::u64 capability_id,
            util::u64 operation,
            util::u64 payload = 0) const noexcept
        {
            return runtime_.capability_call(
                capability_id, operation, payload);
        }

        [[nodiscard]] TrapResult sys_capability_call(
            TrapCapabilityCallView capability) const noexcept
        {
            return runtime_.capability_call(capability);
        }

    private:
        Runtime runtime_{};
    };

    template <typename Runtime>
    [[nodiscard]] auto make_task_syscall_api(
        Runtime runtime) noexcept -> TaskSyscallApi<Runtime>
    {
        return TaskSyscallApi<Runtime>{runtime};
    }
}

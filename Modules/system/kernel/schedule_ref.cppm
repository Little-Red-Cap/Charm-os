module;

export module charm.system.schedule_ref;

import charm.system.clock;
import kernel.eda;
import kernel.evt;

export namespace charm::system {
    using ScheduleFn = bool (*)(void* ctx,
                                kernel::TaskId task,
                                kernel::Event evt,
                                ClockTick due) noexcept;

    class ScheduleRef {
    public:
        constexpr ScheduleRef() noexcept = default;

        static constexpr ScheduleRef raw(ScheduleFn fn, void* ctx) noexcept {
            return ScheduleRef{fn, ctx};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return fn_ != nullptr;
        }

        [[nodiscard]] constexpr ScheduleFn fn() const noexcept {
            return fn_;
        }

        [[nodiscard]] constexpr void* ctx() const noexcept {
            return ctx_;
        }

        [[nodiscard]] constexpr bool schedule_at(kernel::TaskId task,
                                                 kernel::Event evt,
                                                 ClockTick due) const noexcept {
            return fn_ && fn_(ctx_, task, evt, due);
        }

    private:
        constexpr ScheduleRef(ScheduleFn fn, void* ctx) noexcept
            : fn_(fn),
              ctx_(ctx) {}

        ScheduleFn fn_{nullptr};
        void* ctx_{nullptr};
    };

    template <typename Scheduler>
    inline bool scheduler_schedule_at(void* ctx,
                                      kernel::TaskId task,
                                      kernel::Event evt,
                                      ClockTick due) noexcept {
        auto* scheduler = static_cast<Scheduler*>(ctx);
        if (!scheduler) {
            return false;
        }
        return scheduler->schedule_at(due, task, evt);
    }
}

module;

#include <cstddef>

export module kernel.sync_unified;

import kernel.evt;
import kernel.eda;
import kernel.scheduler;
import kernel.sync_base;
import kernel.wait_set;
import kernel.wait_token;
import util.core;

export namespace kernel {
    template <typename Scheduler, util::usize MaxWaiters = 4>
    class SyncUnified {
    public:
        explicit SyncUnified(Scheduler& scheduler) : scheduler_(&scheduler) { }

        [[nodiscard]] bool wait(TaskId task, WaitToken token) noexcept {
            return waiters_.add(task, token);
        }

        [[nodiscard]] bool notify_one(WaitResult result = WaitResult::ok) noexcept {
            TaskId task{};
            WaitToken token{};
            if (!waiters_.pop(task, token)) {
                return false;
            }
            return scheduler_->post(task, make_event(EventId::sync, util::u32(result)));
        }

        [[nodiscard]] bool notify_all(WaitResult result = WaitResult::ok) noexcept {
            bool any = false;
            TaskId task{};
            WaitToken token{};
            while (waiters_.pop(task, token)) {
                (void)scheduler_->post(task, make_event(EventId::sync, util::u32(result)));
                any = true;
            }
            return any;
        }

        [[nodiscard]] bool cancel(WaitToken token) noexcept {
            return waiters_.cancel(token);
        }

    private:
        Scheduler* scheduler_{nullptr};
        WaitSet<MaxWaiters> waiters_{};
    };
}

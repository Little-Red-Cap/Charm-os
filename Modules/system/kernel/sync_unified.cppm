module;

#include <array>
#include <cstddef>

export module kernel.sync_unified;

import kernel.evt;
import kernel.eda;
import kernel.event_token;
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
            return waiters_.add(task, token, EventToken{0});
        }

        [[nodiscard]] bool wait_timeout(TaskId task, WaitToken token, EventToken timeout_token) noexcept {
            return waiters_.add(task, token, timeout_token);
        }

        [[nodiscard]] bool notify_one(WaitResult result = WaitResult::ok) noexcept {
            TaskId task{};
            WaitToken token{};
            EventToken timeout{};
            if (!waiters_.pop(task, token, timeout)) {
                return false;
            }
            if (timeout.value != 0) {
                (void)scheduler_->cancel_event(timeout);
            }
            return scheduler_->post(task, make_event(EventId::sync, util::u32(result)));
        }

        [[nodiscard]] bool notify_all(WaitResult result = WaitResult::ok) noexcept {
            bool any = false;
            TaskId task{};
            WaitToken token{};
            EventToken timeout{};
            while (waiters_.pop(task, token, timeout)) {
                if (timeout.value != 0) {
                    (void)scheduler_->cancel_event(timeout);
                }
                (void)scheduler_->post(task, make_event(EventId::sync, util::u32(result)));
                any = true;
            }
            return any;
        }

        [[nodiscard]] bool cancel(WaitToken token) noexcept {
            TaskId task{};
            EventToken timeout{};
            if (!waiters_.erase(token, task, timeout)) {
                return false;
            }
            if (timeout.value != 0) {
                (void)scheduler_->cancel_event(timeout);
            }
            return scheduler_->post(task, make_event(EventId::sync, util::u32(WaitResult::canceled)));
        }

        [[nodiscard]] bool on_timeout(WaitToken token) noexcept {
            TaskId task{};
            EventToken timeout{};
            if (!waiters_.erase(token, task, timeout)) {
                return false;
            }
            return scheduler_->post(task, make_event(EventId::sync, util::u32(WaitResult::timeout)));
        }

    private:
        Scheduler* scheduler_{nullptr};
        WaitSet<MaxWaiters> waiters_{};
    };
}

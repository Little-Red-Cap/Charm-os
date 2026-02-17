module;

#include <cstddef>

export module kernel.sync_base;

import kernel.evt;
import kernel.eda;
import kernel.scheduler;
import kernel.wait_list;
import util.core;

export namespace kernel {
    enum class WaitResult : util::u32 {
        ok = 1,
        timeout = 2,
        canceled = 3,
        failed = 4
    };

    template <typename Scheduler, util::usize MaxWaiters = 4>
    class SyncBase {
    public:
        explicit SyncBase(Scheduler& scheduler) : scheduler_(&scheduler) { }

        [[nodiscard]] bool pend(TaskId task) noexcept {
            return waiters_.push(task);
        }

        [[nodiscard]] bool post_one(WaitResult result = WaitResult::ok) noexcept {
            TaskId task{};
            if (!waiters_.pop(task)) {
                return false;
            }
            return scheduler_->post(task, make_event(EventId::sync, util::u32(result)));
        }

        [[nodiscard]] bool post_all(WaitResult result = WaitResult::ok) noexcept {
            bool any = false;
            TaskId task{};
            while (waiters_.pop(task)) {
                (void)scheduler_->post(task, make_event(EventId::sync, util::u32(result)));
                any = true;
            }
            return any;
        }

        [[nodiscard]] bool waiting() const noexcept {
            return !waiters_.empty();
        }

        [[nodiscard]] util::usize waiters() const noexcept {
            return waiters_.size();
        }

    private:
        Scheduler* scheduler_{nullptr};
        WaitList<MaxWaiters> waiters_{};
    };
}

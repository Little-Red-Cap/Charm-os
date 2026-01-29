module;

#include <cstddef>

export module kernel.task_api;

import kernel.eda;
import kernel.evt;
import kernel.scheduler;
import kernel.sync_unified;
import kernel.thread_api;
import kernel.sync_base;
import kernel.wait_token;
import util.core;

export namespace kernel {
    template <typename Scheduler>
    class TaskApi {
    public:
        explicit TaskApi(Scheduler& scheduler) : scheduler_(&scheduler), thread_api_(scheduler) { }

        [[nodiscard]] bool sleep(TaskId task, typename Scheduler::TimeSource::Tick due) noexcept {
            return thread_api_.sleep(task, due);
        }

        [[nodiscard]] bool wait(SyncUnified<Scheduler>& sync, TaskId task, WaitToken token) noexcept {
            return sync.wait(task, token);
        }

        [[nodiscard]] bool wait_timeout(
            SyncUnified<Scheduler>& sync,
            TaskId task,
            WaitToken token,
            typename Scheduler::TimeSource::Tick due) noexcept {
            (void)sync.wait(task, token);
            (void)scheduler_->schedule_at_token(due, task, make_event(EventId::sync, util::u32(WaitResult::timeout)));
            return true;
        }

    private:
        Scheduler* scheduler_{nullptr};
        ThreadApi<Scheduler> thread_api_;
    };
}

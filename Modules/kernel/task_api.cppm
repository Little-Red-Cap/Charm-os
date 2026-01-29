module;

#include <cstddef>

export module kernel.task_api;

import kernel.eda;
import kernel.scheduler;
import kernel.sync_unified;
import kernel.thread_api;
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

    private:
        Scheduler* scheduler_{nullptr};
        ThreadApi<Scheduler> thread_api_;
    };
}

module;

#include <cstddef>

export module kernel.thread_api;

import kernel.evt;
import kernel.scheduler;
import kernel.timer;
import kernel.sync_base;
import util.core;

export namespace kernel {
    template <typename Scheduler>
    class ThreadApi {
    public:
        explicit ThreadApi(Scheduler& scheduler) : scheduler_(&scheduler) { }

        [[nodiscard]] bool sleep(TaskId task, typename Scheduler::TimeSource::Tick due) noexcept {
            return scheduler_->schedule_at(due, task, make_event(EventId::tick));
        }

        [[nodiscard]] bool wait(SyncBase<Scheduler>& sync, TaskId task) noexcept {
            return sync.pend(task);
        }

    private:
        Scheduler* scheduler_{nullptr};
    };
}

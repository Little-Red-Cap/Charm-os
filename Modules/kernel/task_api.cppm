module;

#include <cstddef>

export module kernel.task_api;

import kernel.eda;
import kernel.evt;
import kernel.event_token;
import kernel.scheduler;
import kernel.sync_base;
import kernel.sync_unified;
import kernel.task_state;
import kernel.thread_api;
import kernel.wait_token;
import kernel.context;
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
            const auto timeout_token = scheduler_->schedule_at_token(
                due,
                task,
                make_event(EventId::sync, util::u32(WaitResult::timeout)));
            if (timeout_token.value == 0) {
                return false;
            }
            if (!sync.wait_timeout(task, token, timeout_token)) {
                (void)scheduler_->cancel_event(timeout_token);
                return false;
            }
            return true;
        }

        [[nodiscard]] bool cancel_wait(SyncUnified<Scheduler>& sync, WaitToken token) noexcept {
            return sync.cancel(token);
        }

        [[nodiscard]] bool on_timeout(SyncUnified<Scheduler>& sync, WaitToken token) noexcept {
            return sync.on_timeout(token);
        }

        [[nodiscard]] bool enable_task(TaskId task) noexcept {
            return scheduler_->enable_task(task);
        }

        [[nodiscard]] bool disable_task(TaskId task) noexcept {
            return scheduler_->disable_task(task);
        }

        [[nodiscard]] bool disable_current() noexcept {
            if (!has_current()) {
                return false;
            }
            return scheduler_->disable_task(current_task());
        }

        [[nodiscard]] bool stop_task(TaskId task) noexcept {
            return scheduler_->stop_task(task);
        }

        [[nodiscard]] bool stop_current() noexcept {
            if (!has_current()) {
                return false;
            }
            return scheduler_->stop_task(current_task());
        }

        [[nodiscard]] bool restart_task(TaskId task) noexcept {
            return scheduler_->restart_task(task);
        }

        [[nodiscard]] TaskState state_of(TaskId task) const noexcept {
            return scheduler_->state_of(task);
        }

        [[nodiscard]] bool is_enabled(TaskId task) const noexcept {
            return scheduler_->is_enabled(task);
        }

    private:
        Scheduler* scheduler_{nullptr};
        ThreadApi<Scheduler> thread_api_;
    };
}

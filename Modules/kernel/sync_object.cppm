module;

#include <cstddef>

export module kernel.sync_object;

import kernel.evt;
import kernel.eda;
import kernel.scheduler;
import util.core;

export namespace kernel {
    enum class SyncStatus : util::u32 {
        success = 1,
        timeout = 2,
        failed = 3
    };

    template <typename Scheduler>
    class SyncObject {
    public:
        explicit SyncObject(Scheduler& scheduler) : scheduler_(&scheduler) { }

        [[nodiscard]] bool pend(TaskId task) noexcept {
            if (waiting_) {
                return false;
            }
            waiting_ = true;
            waiter_ = task;
            return true;
        }

        [[nodiscard]] bool post(SyncStatus status = SyncStatus::success) noexcept {
            if (!waiting_) {
                return false;
            }
            waiting_ = false;
            return scheduler_->post(waiter_, make_event(EventId::sync, util::u32(status)));
        }

        [[nodiscard]] bool waiting() const noexcept {
            return waiting_;
        }

    private:
        Scheduler* scheduler_{nullptr};
        TaskId waiter_{};
        bool waiting_{false};
    };
}

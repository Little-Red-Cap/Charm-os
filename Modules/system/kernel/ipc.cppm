module;

#include <cstddef>

export module kernel.ipc;

import kernel.evt;
import kernel.eda;
import kernel.scheduler;
import kernel.sync;
import kernel.sync_base;
import service.ring_queue;
import util.core;

export namespace kernel {
    template <typename Caps, util::usize MaxCount, typename Scheduler, util::usize MaxWaiters = 4>
    class SemaphoreIpc {
    public:
        explicit SemaphoreIpc(Scheduler& scheduler) : scheduler_(&scheduler), sync_(scheduler) { }

        [[nodiscard]] bool wait(TaskId task) noexcept {
            if (sem_.try_acquire()) {
                return true;
            }
            return sync_.pend(task);
        }

        [[nodiscard]] bool post() noexcept {
            if (sync_.waiting()) {
                return sync_.post_one(WaitResult::ok);
            }
            return sem_.release();
        }

    private:
        Scheduler* scheduler_{nullptr};
        Semaphore<Caps, MaxCount> sem_{};
        SyncBase<Scheduler, MaxWaiters> sync_;
    };

    template <typename Scheduler, typename T, util::usize Capacity>
    class QueueIpc {
    public:
        explicit QueueIpc(Scheduler& scheduler, service::RingQueue<T, Capacity>& queue)
            : scheduler_(&scheduler), queue_(&queue) { }

        [[nodiscard]] bool send(TaskId task, T value) noexcept {
            if (!queue_->push(value)) {
                return false;
            }
            return scheduler_->post_demand(task, make_event(EventId::message, util::u32(1)));
        }

        [[nodiscard]] bool recv(T& out) noexcept {
            auto value = queue_->pop();
            if (!value.has_value()) {
                return false;
            }
            out = *value;
            return true;
        }

    private:
        Scheduler* scheduler_{nullptr};
        service::RingQueue<T, Capacity>* queue_{nullptr};
    };

    template <typename Scheduler, util::usize MaxWaiters = 4>
    class TriggerIpc {
    public:
        explicit TriggerIpc(Scheduler& scheduler) : sync_(scheduler) { }

        [[nodiscard]] bool wait(TaskId task) noexcept {
            return sync_.pend(task);
        }

        [[nodiscard]] bool set() noexcept {
            return sync_.post_all(WaitResult::ok);
        }

    private:
        SyncBase<Scheduler, MaxWaiters> sync_;
    };
}

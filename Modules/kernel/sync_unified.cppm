module;

#include <array>
#include <cstddef>

export module kernel.sync_unified;

import kernel.evt;
import kernel.eda;
import kernel.event_token;
import kernel.scheduler;
import kernel.sync_base;
import kernel.wait_token;
import util.core;

export namespace kernel {
    template <typename Scheduler, util::usize MaxWaiters = 4>
    class SyncUnified {
    public:
        explicit SyncUnified(Scheduler& scheduler) : scheduler_(&scheduler) { }

        [[nodiscard]] bool wait(TaskId task, WaitToken token) noexcept {
            return insert(task, token, EventToken{0});
        }

        [[nodiscard]] bool wait_timeout(TaskId task, WaitToken token, EventToken timeout_token) noexcept {
            return insert(task, token, timeout_token);
        }

        [[nodiscard]] bool notify_one(WaitResult result = WaitResult::ok) noexcept {
            if (count_ == 0) {
                return false;
            }
            const Entry entry = entries_[head_];
            head_ = (head_ + 1) % MaxWaiters;
            --count_;
            if (entry.timeout.value != 0) {
                (void)scheduler_->cancel_event(entry.timeout);
            }
            return scheduler_->post(entry.task, make_event(EventId::sync, util::u32(result)));
        }

        [[nodiscard]] bool notify_all(WaitResult result = WaitResult::ok) noexcept {
            bool any = false;
            while (count_ > 0) {
                any = notify_one(result) || any;
            }
            return any;
        }

        [[nodiscard]] bool cancel(WaitToken token) noexcept {
            if (count_ == 0) {
                return false;
            }
            std::array<Entry, MaxWaiters> new_entries{};
            util::usize new_count = 0;
            util::usize idx = head_;
            for (util::usize i = 0; i < count_; ++i) {
                const auto& entry = entries_[idx];
                if (entry.token.value != token.value) {
                    new_entries[new_count++] = entry;
                } else if (entry.timeout.value != 0) {
                    (void)scheduler_->cancel_event(entry.timeout);
                }
                idx = (idx + 1) % MaxWaiters;
            }
            const bool removed = new_count != count_;
            entries_ = new_entries;
            head_ = 0;
            tail_ = new_count % MaxWaiters;
            count_ = new_count;
            return removed;
        }

    private:
        struct Entry {
            TaskId task{};
            WaitToken token{};
            EventToken timeout{};
        };

        [[nodiscard]] bool insert(TaskId task, WaitToken token, EventToken timeout) noexcept {
            if (count_ >= MaxWaiters) {
                return false;
            }
            entries_[tail_] = {task, token, timeout};
            tail_ = (tail_ + 1) % MaxWaiters;
            ++count_;
            return true;
        }

        Scheduler* scheduler_{nullptr};
        std::array<Entry, MaxWaiters> entries_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };
}

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
            return waiters_.add(task, token);
        }

        [[nodiscard]] bool wait_timeout(TaskId task, WaitToken token, EventToken timeout_token) noexcept {
            if (!waiters_.add(task, token)) {
                return false;
            }
            record_timeout(token, timeout_token);
            return true;
        }

        [[nodiscard]] bool notify_one(WaitResult result = WaitResult::ok) noexcept {
            TaskId task{};
            WaitToken token{};
            if (!waiters_.pop(task, token)) {
                return false;
            }
            cancel_timeout(token);
            return scheduler_->post(task, make_event(EventId::sync, util::u32(result)));
        }

        [[nodiscard]] bool notify_all(WaitResult result = WaitResult::ok) noexcept {
            bool any = false;
            TaskId task{};
            WaitToken token{};
            while (waiters_.pop(task, token)) {
                cancel_timeout(token);
                (void)scheduler_->post(task, make_event(EventId::sync, util::u32(result)));
                any = true;
            }
            return any;
        }

        [[nodiscard]] bool cancel(WaitToken token) noexcept {
            TaskId task{};
            if (!waiters_.erase(token, task)) {
                return false;
            }
            cancel_timeout(token);
            return scheduler_->post(task, make_event(EventId::sync, util::u32(WaitResult::canceled)));
        }

        [[nodiscard]] bool on_timeout(WaitToken token) noexcept {
            TaskId task{};
            if (!waiters_.erase(token, task)) {
                return false;
            }
            cancel_timeout(token);
            return scheduler_->post(task, make_event(EventId::sync, util::u32(WaitResult::timeout)));
        }

    private:
        struct TimeoutEntry {
            WaitToken token{};
            EventToken timeout{};
        };

        void record_timeout(WaitToken token, EventToken timeout_token) noexcept {
            for (auto& entry : timeout_tokens_) {
                if (entry.token.value == token.value) {
                    entry.timeout = timeout_token;
                    return;
                }
            }
            for (auto& entry : timeout_tokens_) {
                if (entry.timeout.value == 0) {
                    entry.token = token;
                    entry.timeout = timeout_token;
                    return;
                }
            }
            auto& entry = timeout_tokens_[token_slot_];
            if (entry.timeout.value != 0) {
                (void)scheduler_->cancel_event(entry.timeout);
            }
            entry.token = token;
            entry.timeout = timeout_token;
            token_slot_ = (token_slot_ + 1) % MaxWaiters;
        }

        void cancel_timeout(WaitToken token) noexcept {
            for (auto& entry : timeout_tokens_) {
                if (entry.token.value == token.value && entry.timeout.value != 0) {
                    (void)scheduler_->cancel_event(entry.timeout);
                    entry.timeout = EventToken{0};
                    return;
                }
            }
        }

        Scheduler* scheduler_{nullptr};
        WaitSet<MaxWaiters> waiters_{};
        std::array<TimeoutEntry, MaxWaiters> timeout_tokens_{};
        util::usize token_slot_{0};
    };
}

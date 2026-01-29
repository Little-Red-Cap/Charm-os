module;

#include <array>
#include <cstddef>

export module kernel.wait_set;

import kernel.evt;
import kernel.eda;
import kernel.sync_base;
import kernel.wait_token;
import util.core;

export namespace kernel {
    template <util::usize Capacity>
    class WaitSet {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool add(TaskId task, WaitToken token) noexcept {
            if (count_ >= Capacity) {
                return false;
            }
            entries_[tail_] = {task, token};
            tail_ = (tail_ + 1) % Capacity;
            ++count_;
            return true;
        }

        [[nodiscard]] bool pop(TaskId& task, WaitToken& token) noexcept {
            if (count_ == 0) {
                return false;
            }
            const auto entry = entries_[head_];
            task = entry.task;
            token = entry.token;
            head_ = (head_ + 1) % Capacity;
            --count_;
            return true;
        }

        [[nodiscard]] bool cancel(WaitToken token) noexcept {
            if (count_ == 0) {
                return false;
            }
            std::array<Entry, Capacity> new_entries{};
            util::usize new_count = 0;
            util::usize idx = head_;
            for (util::usize i = 0; i < count_; ++i) {
                const auto& entry = entries_[idx];
                if (entry.token.value != token.value) {
                    new_entries[new_count++] = entry;
                }
                idx = (idx + 1) % Capacity;
            }
            const bool removed = new_count != count_;
            entries_ = new_entries;
            head_ = 0;
            tail_ = new_count % Capacity;
            count_ = new_count;
            return removed;
        }

        [[nodiscard]] bool empty() const noexcept {
            return count_ == 0;
        }

        [[nodiscard]] util::usize size() const noexcept {
            return count_;
        }

    private:
        struct Entry {
            TaskId task{};
            WaitToken token{};
        };

        std::array<Entry, Capacity> entries_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };
}

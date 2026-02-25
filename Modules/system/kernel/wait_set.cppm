module;

#include <array>
#include <cstddef>

export module kernel.wait_set;

import kernel.eda;
import kernel.wait_token;
import util.core;

export namespace kernel {
    template <util::usize Capacity>
    class WaitSet {
    public:
        static_assert(Capacity >= 1);

        struct Entry {
            TaskId task{};
            WaitToken token{};
        };

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

        [[nodiscard]] bool erase(WaitToken token, TaskId& task) noexcept {
            if (count_ == 0) {
                return false;
            }
            bool found = false;
            util::usize write = 0;
            for (util::usize i = 0; i < count_; ++i) {
                const auto idx = (head_ + i) % Capacity;
                const auto& entry = entries_[idx];
                if (!found && entry.token.value == token.value) {
                    task = entry.task;
                    found = true;
                } else {
                    entries_[(head_ + write) % Capacity] = entry;
                    ++write;
                }
            }
            count_ = write;
            tail_ = (head_ + count_) % Capacity;
            return found;
        }

        [[nodiscard]] bool empty() const noexcept {
            return count_ == 0;
        }

        [[nodiscard]] util::usize size() const noexcept {
            return count_;
        }

    private:
        std::array<Entry, Capacity> entries_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };
}

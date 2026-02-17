module;

#include <array>
#include <cstddef>

export module kernel.wait_list;

import kernel.eda;
import util.core;

export namespace kernel {
    template <util::usize Capacity>
    class WaitList {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool push(TaskId task) noexcept {
            if (count_ >= Capacity) {
                return false;
            }
            tasks_[tail_] = task;
            tail_ = (tail_ + 1) % Capacity;
            ++count_;
            return true;
        }

        [[nodiscard]] bool pop(TaskId& out) noexcept {
            if (count_ == 0) {
                return false;
            }
            out = tasks_[head_];
            head_ = (head_ + 1) % Capacity;
            --count_;
            return true;
        }

        [[nodiscard]] bool empty() const noexcept {
            return count_ == 0;
        }

        [[nodiscard]] util::usize size() const noexcept {
            return count_;
        }

    private:
        std::array<TaskId, Capacity> tasks_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };
}

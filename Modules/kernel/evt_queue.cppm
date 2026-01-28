module;

#include <array>
#include <cstddef>
#include <optional>

export module kernel.evt_queue;

import kernel.evt;
import kernel.eda;
import util.core;

export namespace kernel {
    struct EventNode {
        TaskId task{};
        Event event{};
    };

    template <util::usize Capacity>
    class EventQueue {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool push(EventNode node) noexcept {
            if (count_ >= Capacity) {
                return false;
            }
            buffer_[tail_] = node;
            tail_ = (tail_ + 1) % Capacity;
            ++count_;
            return true;
        }

        [[nodiscard]] std::optional<EventNode> pop() noexcept {
            if (count_ == 0) {
                return std::nullopt;
            }
            EventNode node = buffer_[head_];
            head_ = (head_ + 1) % Capacity;
            --count_;
            return node;
        }

        [[nodiscard]] bool empty() const noexcept {
            return count_ == 0;
        }

        [[nodiscard]] util::usize size() const noexcept {
            return count_;
        }

    private:
        std::array<EventNode, Capacity> buffer_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };
}

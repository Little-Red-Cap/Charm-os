module;

#include <array>
#include <cstddef>
#include <optional>

export module kernel.event_queue;

import kernel.evt;
import kernel.eda;
import util.core;

export namespace kernel {
    struct EventNode {
        TaskId task{};
        Event event{};
        util::u64 tag{0};
    };

    template <util::usize Capacity>
    class EventQueue {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool push(EventNode node) noexcept {
            if (count_ >= Capacity) {
                return false;
            }
            nodes_[tail_] = node;
            tail_ = (tail_ + 1) % Capacity;
            ++count_;
            return true;
        }

        [[nodiscard]] std::optional<EventNode> pop() noexcept {
            if (count_ == 0) {
                return std::nullopt;
            }
            EventNode node = nodes_[head_];
            head_ = (head_ + 1) % Capacity;
            --count_;
            return node;
        }

        [[nodiscard]] bool cancel(util::u64 tag) noexcept {
            if (count_ == 0) {
                return false;
            }
            std::array<EventNode, Capacity> new_nodes{};
            util::usize new_count = 0;
            util::usize idx = head_;
            for (util::usize i = 0; i < count_; ++i) {
                const auto& node = nodes_[idx];
                if (node.tag != tag) {
                    new_nodes[new_count++] = node;
                }
                idx = (idx + 1) % Capacity;
            }
            nodes_ = new_nodes;
            head_ = 0;
            tail_ = new_count % Capacity;
            const bool removed = new_count != count_;
            count_ = new_count;
            return removed;
        }

        [[nodiscard]] util::usize size() const noexcept {
            return count_;
        }

        [[nodiscard]] bool empty() const noexcept {
            return count_ == 0;
        }

    private:
        std::array<EventNode, Capacity> nodes_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };
}

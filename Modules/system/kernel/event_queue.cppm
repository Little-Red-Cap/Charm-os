module;

#include <array>
#include <cstddef>
#include <optional>

export module kernel.event_queue;

import kernel.evt;
import kernel.eda;
import util.core;

export namespace kernel {
    enum class DropPolicy : unsigned char {
        drop_newest,
        drop_oldest
    };
    struct EventNode {
        TaskId task{};
        Event event{};
        util::u64 tag{0};
    };

    template <util::usize Capacity>
    class EventQueue {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool push(EventNode node, DropPolicy policy = DropPolicy::drop_newest) noexcept {
            if (count_ >= Capacity) {
                if (policy == DropPolicy::drop_oldest) {
                    head_ = (head_ + 1) % Capacity;
                    --count_;
                } else {
                    return false;
                }
            }
            nodes_[tail_] = node;
            tail_ = (tail_ + 1) % Capacity;
            ++count_;
            return true;
        }

        [[nodiscard]] bool coalesce(TaskId task, EventId id, Event evt, util::u64 tag) noexcept {
            if (count_ == 0) {
                return false;
            }
            bool removed = false;
            util::usize write = 0;
            for (util::usize i = 0; i < count_; ++i) {
                const auto idx = (head_ + i) % Capacity;
                const auto& node = nodes_[idx];
                if (node.task.value == task.value && node.event.id == id) {
                    removed = true;
                } else {
                    nodes_[(head_ + write) % Capacity] = node;
                    ++write;
                }
            }
            if (!removed) {
                return false;
            }
            nodes_[(head_ + write) % Capacity] = EventNode{task, evt, tag};
            ++write;
            count_ = write;
            tail_ = (head_ + count_) % Capacity;
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
            util::usize write = 0;
            for (util::usize i = 0; i < count_; ++i) {
                const auto idx = (head_ + i) % Capacity;
                const auto& node = nodes_[idx];
                if (node.tag != tag) {
                    nodes_[(head_ + write) % Capacity] = node;
                    ++write;
                }
            }
            const bool removed = write != count_;
            count_ = write;
            tail_ = (head_ + count_) % Capacity;
            return removed;
        }

        [[nodiscard]] bool drop_task(TaskId task) noexcept {
            if (count_ == 0) {
                return false;
            }
            util::usize write = 0;
            for (util::usize i = 0; i < count_; ++i) {
                const auto idx = (head_ + i) % Capacity;
                const auto& node = nodes_[idx];
                if (node.task.value != task.value) {
                    nodes_[(head_ + write) % Capacity] = node;
                    ++write;
                }
            }
            const bool removed = write != count_;
            count_ = write;
            tail_ = (head_ + count_) % Capacity;
            return removed;
        }

        [[nodiscard]] bool drop_task_with_tag(TaskId task, util::u64 tag) noexcept {
            if (count_ == 0) {
                return false;
            }
            util::usize write = 0;
            for (util::usize i = 0; i < count_; ++i) {
                const auto idx = (head_ + i) % Capacity;
                const auto& node = nodes_[idx];
                if (node.task.value != task.value || node.tag != tag) {
                    nodes_[(head_ + write) % Capacity] = node;
                    ++write;
                }
            }
            const bool removed = write != count_;
            count_ = write;
            tail_ = (head_ + count_) % Capacity;
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

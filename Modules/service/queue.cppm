module;

#include <cstddef>
#include <optional>

export module service.queue;

import util.core;
import service.ring_queue;

export namespace service {
    template <typename T, util::usize Capacity>
    class Queue {
    public:
        [[nodiscard]] bool push(const T& value) noexcept {
            return queue_.push(value);
        }

        [[nodiscard]] std::optional<T> pop() noexcept {
            return queue_.pop();
        }

        [[nodiscard]] util::usize size() const noexcept {
            return queue_.size();
        }

        [[nodiscard]] bool empty() const noexcept {
            return queue_.empty();
        }

    private:
        RingQueue<T, Capacity> queue_{};
    };
}

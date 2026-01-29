module;

#include <array>
#include <cstddef>
#include <optional>

export module service.ring_queue;

import util.core;

export namespace service {
    template <typename T, util::usize Capacity>
    class RingQueue {
    public:
        static_assert(Capacity >= 1);

        [[nodiscard]] bool push(T value) noexcept {
            if (count_ >= Capacity) {
                return false;
            }
            buffer_[tail_] = value;
            tail_ = (tail_ + 1) % Capacity;
            ++count_;
            return true;
        }

        [[nodiscard]] std::optional<T> pop() noexcept {
            if (count_ == 0) {
                return std::nullopt;
            }
            T value = buffer_[head_];
            head_ = (head_ + 1) % Capacity;
            --count_;
            return value;
        }

        [[nodiscard]] util::usize size() const noexcept {
            return count_;
        }

        [[nodiscard]] bool empty() const noexcept {
            return count_ == 0;
        }

    private:
        std::array<T, Capacity> buffer_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize count_{0};
    };
}

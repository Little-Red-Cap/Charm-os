module;

#include <array>
#include <cstddef>
#include <cstdint>

export module service_ring_buffer;

import util.core;

export namespace service {
    template <typename T, util::usize Capacity>
    class RingBuffer {
    public:
        constexpr RingBuffer() = default;

        [[nodiscard]] constexpr bool push(const T& value) noexcept {
            if (full()) {
                return false;
            }
            data_[tail_] = value;
            tail_ = (tail_ + 1) % Capacity;
            ++size_;
            return true;
        }

        [[nodiscard]] constexpr bool pop(T& out) noexcept {
            if (empty()) {
                return false;
            }
            out = data_[head_];
            head_ = (head_ + 1) % Capacity;
            --size_;
            return true;
        }

        [[nodiscard]] constexpr bool empty() const noexcept { return size_ == 0; }
        [[nodiscard]] constexpr bool full() const noexcept { return size_ == Capacity; }
        [[nodiscard]] constexpr util::usize size() const noexcept { return size_; }
        [[nodiscard]] constexpr util::usize capacity() const noexcept { return Capacity; }

        constexpr void clear() noexcept {
            head_ = 0;
            tail_ = 0;
            size_ = 0;
        }

    private:
        std::array<T, Capacity> data_{};
        util::usize head_{0};
        util::usize tail_{0};
        util::usize size_{0};
    };
}

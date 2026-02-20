//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>
#include <optional>

export module input.queue;

import service.ring_queue;

export namespace input
{
    template <class T, std::uint16_t N>
    class RingQueue {
    public:
        bool push(const T& v) noexcept
        {
            return queue_.push(v);
        }

        std::optional<T> pop() noexcept
        {
            return queue_.pop();
        }

        void          clear() noexcept { queue_ = service::RingQueue<T, N>{}; }
        std::uint16_t size() const noexcept { return (std::uint16_t)queue_.size(); }
        bool          empty() const noexcept { return queue_.empty(); }
        bool          full() const noexcept { return size() >= N; }

    private:
        service::RingQueue<T, N> queue_{};
    };
} // namespace input

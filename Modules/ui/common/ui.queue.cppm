module;
#include <cstdint>
#include <optional>
export module ui.queue;

import service_ring_buffer;

export namespace ui {
    template <class T, std::uint16_t N>
    class RingQueue {
    public:
        bool push(const T& v) noexcept
        {
            return queue_.push(v);
        }

        std::optional<T> pop() noexcept
        {
            T value{};
            if (!queue_.pop(value)) {
                return std::nullopt;
            }
            return value;
        }

        void          clear() noexcept { queue_.clear(); }
        std::uint16_t size() const noexcept { return (std::uint16_t)queue_.size(); }
        bool          empty() const noexcept { return queue_.empty(); }
        bool          full() const noexcept { return queue_.full(); }

    private:
        service::RingBuffer<T, N> queue_{};
    };
}

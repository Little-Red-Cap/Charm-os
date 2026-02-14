module;

#include <array>
#include <cstddef>
#include <cstdint>

export module service_trace;

import util.core;

export namespace service {
    template <typename Tick, util::usize Capacity>
    struct TraceRecord {
        Tick time{};
        util::u32 id{0};
        util::u64 payload{0};
        util::u32 count{1};
    };

    template <typename Tick, util::usize Capacity>
    class TraceBuffer {
    public:
        constexpr TraceBuffer() = default;

        [[nodiscard]] constexpr util::usize size() const noexcept { return size_; }
        [[nodiscard]] constexpr util::usize capacity() const noexcept { return Capacity; }
        [[nodiscard]] constexpr util::usize head() const noexcept { return head_; }
        [[nodiscard]] constexpr const std::array<TraceRecord<Tick, Capacity>, Capacity>& data() const noexcept {
            return data_;
        }

        constexpr void clear() noexcept {
            size_ = 0;
            head_ = 0;
        }

        constexpr void push(const TraceRecord<Tick, Capacity>& rec) noexcept {
            data_[head_] = rec;
            head_ = (head_ + 1) % Capacity;
            if (size_ < Capacity) {
                ++size_;
            }
        }

    private:
        std::array<TraceRecord<Tick, Capacity>, Capacity> data_{};
        util::usize size_{0};
        util::usize head_{0};
    };
}

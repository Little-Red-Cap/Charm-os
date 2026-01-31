module;

// Optional/experimental module: trace buffer.
#include <array>
#include <cstddef>

export module kernel.trace;

import kernel.eda;
import kernel.evt;
import util.core;

export namespace kernel {
    template <typename Tick>
    struct TraceRecord {
        Tick time{};
        TaskId task{};
        EventId id{};
        util::u64 payload{};
        util::u32 count{1};
    };

    template <typename Tick, std::size_t Capacity>
    class TraceBuffer {
    public:
        static_assert(Capacity >= 1);

        void push(TraceRecord<Tick> rec) noexcept {
            records_[head_] = rec;
            head_ = (head_ + 1) % Capacity;
            if (count_ < Capacity) {
                ++count_;
            }
        }

        [[nodiscard]] std::size_t size() const noexcept {
            return count_;
        }

        [[nodiscard]] const std::array<TraceRecord<Tick>, Capacity>& data() const noexcept {
            return records_;
        }

        [[nodiscard]] std::size_t head() const noexcept {
            return head_;
        }

    private:
        std::array<TraceRecord<Tick>, Capacity> records_{};
        std::size_t head_{0};
        std::size_t count_{0};
    };
}

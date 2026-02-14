module;

// Optional/experimental module: trace buffer.
#include <array>
#include <cstddef>

export module kernel.trace;

import kernel.eda;
import kernel.evt;
import util.core;
import trace_core;

export namespace kernel {
    using TraceKind = trace::TraceKind;
    using TraceStats = trace::TraceStats;

    template <typename Tick>
    struct TraceRecord {
        Tick time{};
        TaskId task{};
        EventId id{};
        util::u64 payload{};
        util::u32 count{1};
        TraceKind kind{TraceKind::event};
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

    template <typename Tick>
    class TraceAggregator {
    public:
        void clear() noexcept {
            totals_ = {};
            in_span_ = false;
            last_span_begin_ = Tick{};
        }

        void observe(const TraceRecord<Tick>& rec) noexcept {
            switch (rec.kind) {
            case TraceKind::event:
                ++totals_.events;
                break;
            case TraceKind::counter:
                ++totals_.counters;
                break;
            case TraceKind::span_begin:
                ++totals_.span_begin;
                in_span_ = true;
                last_span_begin_ = rec.time;
                break;
            case TraceKind::span_end:
                ++totals_.span_end;
                if (in_span_) {
                    const auto dur = rec.time - last_span_begin_;
                    totals_.span_total += static_cast<util::u64>(dur);
                    if (static_cast<util::u64>(dur) > totals_.span_max) {
                        totals_.span_max = static_cast<util::u64>(dur);
                    }
                    in_span_ = false;
                }
                break;
            }
        }

        const TraceStats& totals() const noexcept { return totals_; }

    private:
        TraceStats totals_{};
        bool in_span_{false};
        Tick last_span_begin_{};
    };
}

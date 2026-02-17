module;

#include <array>
#include <cstddef>
#include <cstdint>

export module service_trace;

import util.core;
import trace_core;

export namespace service {
    using TraceKind = trace::TraceKind;

    template <typename Tick, util::usize Capacity>
    struct TraceRecord {
        Tick time{};
        util::u32 id{0};
        util::u64 payload{0};
        util::u32 count{1};
        TraceKind kind{TraceKind::event};
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

    using TraceStats = trace::TraceStats;

    template <typename Tick>
    struct TraceIdStat {
        util::u32 id{0};
        bool used{false};
        util::u32 events{0};
        util::u32 counters{0};
        util::u32 counter_delta{0};
        util::u32 spans{0};
        util::u32 event_count{0};
        util::u64 counter_value{0};
        util::u64 counter_sum{0};
        util::u64 counter_max{0};
        util::u64 counter_min{0};
        bool in_span{false};
        Tick last_span_begin{};
        Tick last_span_duration{};
        util::u32 span_count{0};
        util::u64 span_total{0};
        util::u64 span_max{0};
        util::u32 span_pair{0};
    };

    template <typename Tick, util::usize MaxIds>
    class TraceAggregator {
    public:
        constexpr TraceAggregator() = default;

        void clear() noexcept {
            totals_ = {};
            for (auto& s : stats_) s = {};
        }

        template <util::usize Capacity>
        void observe(const TraceRecord<Tick, Capacity>& rec) noexcept {
            auto& stat = find_or_alloc(rec.id);
            switch (rec.kind) {
            case TraceKind::event:
                trace::observe_totals(totals_, TraceKind::event);
                ++stat.events;
                ++stat.event_count;
                break;
            case TraceKind::counter:
                trace::observe_totals(totals_, TraceKind::counter);
                ++stat.counters;
                stat.counter_value = rec.payload;
                stat.counter_sum += rec.payload;
                if (stat.counters == 1 || rec.payload < stat.counter_min) {
                    stat.counter_min = rec.payload;
                }
                if (rec.payload > stat.counter_max) stat.counter_max = rec.payload;
                break;
            case TraceKind::counter_delta:
                trace::observe_totals(totals_, TraceKind::counter_delta);
                ++stat.counters;
                ++stat.counter_delta;
                stat.counter_value += rec.payload;
                stat.counter_sum += rec.payload;
                if (stat.counters == 1 || stat.counter_value < stat.counter_min) {
                    stat.counter_min = stat.counter_value;
                }
                if (stat.counter_value > stat.counter_max) stat.counter_max = stat.counter_value;
                break;
            case TraceKind::span_begin:
                trace::observe_totals(totals_, TraceKind::span_begin);
                ++stat.spans;
                stat.in_span = true;
                stat.last_span_begin = rec.time;
                break;
            case TraceKind::span_end:
                if (stat.in_span) {
                    stat.in_span = false;
                    stat.last_span_duration = rec.time - stat.last_span_begin;
                    stat.span_total += stat.last_span_duration;
                    if (stat.last_span_duration > stat.span_max) stat.span_max = stat.last_span_duration;
                    trace::observe_totals(totals_, TraceKind::span_end,
                        static_cast<util::u64>(stat.last_span_duration));
                    ++stat.span_count;
                } else {
                    trace::observe_totals(totals_, TraceKind::span_end);
                }
                break;
            case TraceKind::span_pair:
                ++stat.spans;
                ++stat.span_pair;
                stat.last_span_duration = static_cast<Tick>(rec.payload);
                stat.span_total += stat.last_span_duration;
                if (stat.last_span_duration > stat.span_max) stat.span_max = stat.last_span_duration;
                trace::observe_totals(totals_, TraceKind::span_pair, static_cast<util::u64>(rec.payload));
                ++stat.span_count;
                break;
            }
        }

        const TraceStats& totals() const noexcept { return totals_; }
        const std::array<TraceIdStat<Tick>, MaxIds>& by_id() const noexcept { return stats_; }

    private:
        TraceIdStat<Tick>& find_or_alloc(util::u32 id) noexcept {
            for (auto& s : stats_) {
                if (s.used && s.id == id) return s;
            }
            for (auto& s : stats_) {
                if (!s.used) {
                    s.used = true;
                    s.id = id;
                    return s;
                }
            }
            return stats_[0];
        }

        TraceStats totals_{};
        std::array<TraceIdStat<Tick>, MaxIds> stats_{};
    };
}

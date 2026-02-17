module;

#include <cstdint>

export module trace_core;

import util.core;

export namespace trace {
    enum class TraceKind : util::u8 {
        event = 0,
        counter = 1,
        span_begin = 2,
        span_end = 3,
        counter_delta = 4,
        span_pair = 5
    };

    struct TraceStats {
        util::u32 events{0};
        util::u32 counters{0};
        util::u32 counter_delta{0};
        util::u32 span_begin{0};
        util::u32 span_end{0};
        util::u32 span_pair{0};
        util::u64 span_total{0};
        util::u64 span_max{0};
    };

    inline void observe_totals(TraceStats& totals, TraceKind kind, util::u64 span_duration = 0) noexcept {
        switch (kind) {
        case TraceKind::event:
            ++totals.events;
            break;
        case TraceKind::counter:
            ++totals.counters;
            break;
        case TraceKind::counter_delta:
            ++totals.counters;
            ++totals.counter_delta;
            break;
        case TraceKind::span_begin:
            ++totals.span_begin;
            break;
        case TraceKind::span_end:
            ++totals.span_end;
            if (span_duration > 0) {
                totals.span_total += span_duration;
                if (span_duration > totals.span_max) totals.span_max = span_duration;
            }
            break;
        case TraceKind::span_pair:
            ++totals.span_pair;
            ++totals.span_begin;
            ++totals.span_end;
            if (span_duration > 0) {
                totals.span_total += span_duration;
                if (span_duration > totals.span_max) totals.span_max = span_duration;
            }
            break;
        }
    }
}

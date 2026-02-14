module;

#include <cstdint>

export module trace_core;

import util.core;

export namespace trace {
    enum class TraceKind : util::u8 {
        event = 0,
        counter = 1,
        span_begin = 2,
        span_end = 3
    };

    struct TraceStats {
        util::u32 events{0};
        util::u32 counters{0};
        util::u32 span_begin{0};
        util::u32 span_end{0};
        util::u64 span_total{0};
        util::u64 span_max{0};
    };
}

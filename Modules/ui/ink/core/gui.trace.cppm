module;

#include <cstddef>
#include <cstdint>

export module gui.trace;

import service_trace;
import trace_core;
import util.core;

export namespace gui::trace {
    enum class TraceId : util::u32 {
        TreeBeginFrame = 1,
        TreeNodeBegin = 2,
        FocusSyncReasonBase = 100,
        InputIntentBase = 200,
    };

    inline constexpr util::usize kTraceCapacity = 512;
    using TraceRecord = service::TraceRecord<util::u32, kTraceCapacity>;
    using TraceBuffer = service::TraceBuffer<util::u32, kTraceCapacity>;

    inline TraceBuffer& buffer_mut() noexcept {
        static TraceBuffer buffer{};
        return buffer;
    }

    inline const TraceBuffer& buffer() noexcept {
        return buffer_mut();
    }

    inline void clear() noexcept {
        buffer_mut().clear();
    }

    inline void trace_emit(::trace::TraceKind kind,
                           TraceId id,
                           util::u64 payload,
                           util::u32 count = 1) noexcept {
        static util::u32 seq{0};
        TraceRecord rec{};
        rec.time = ++seq;
        rec.id = static_cast<util::u32>(id);
        rec.payload = payload;
        rec.count = count;
        rec.kind = kind;
        buffer_mut().push(rec);
    }

    inline void trace_counter(TraceId id, util::u64 payload) noexcept {
        trace_emit(::trace::TraceKind::counter, id, payload);
    }

    inline void trace_counter_delta(TraceId id, util::u64 delta) noexcept {
        trace_emit(::trace::TraceKind::counter_delta, id, delta);
    }
}

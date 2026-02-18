module;

#include <cstddef>
#include <cstdint>

export module input.trace;

import input.raw;
import service_trace;
import trace_core;
import util.core;

export namespace input::trace {
    enum class TraceId : util::u32 {
        RawButtonBase = 1,
        RawPointerDown = 10,
        RawPointerMove = 11,
        RawPointerUp = 12,
        RawEncoder = 20,
    };

    inline constexpr util::usize kTraceCapacity = 256;
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

    inline TraceId raw_button_id(Button b) noexcept {
        return static_cast<TraceId>(static_cast<util::u32>(TraceId::RawButtonBase)
            + static_cast<util::u32>(b));
    }

    inline void trace_counter_delta(TraceId id, util::u64 delta) noexcept {
        static util::u32 seq{0};
        TraceRecord rec{};
        rec.time = ++seq;
        rec.id = static_cast<util::u32>(id);
        rec.payload = delta;
        rec.count = 1;
        rec.kind = service::TraceKind::counter_delta;
        buffer_mut().push(rec);
    }
}

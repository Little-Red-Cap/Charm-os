export module power.trace;

import util.core;
import trace_core;
import power.types;

export namespace power::trace {
    struct Sink {
        void* ctx{nullptr};
        void (*emit)(void*, ::trace::TraceKind, util::u32, util::u64) noexcept { nullptr };
    };

    enum class EventId : util::u16 {
        request_state = 1,
        enter_state = 2,
        exit_state = 3,
        wake_source_add = 4,
        wake_source_remove = 5,
        clock_domain_add = 6,
        clock_domain_remove = 7
    };

    Sink& sink() noexcept;
    void set_sink(Sink s) noexcept;
    void record(EventId id,
                util::u64 payload = 0,
                ::trace::TraceKind kind = ::trace::TraceKind::event) noexcept;
}

namespace power::trace::detail {
    Sink g_sink{};
}

namespace power::trace {
    Sink& sink() noexcept {
        return detail::g_sink;
    }

    void set_sink(Sink s) noexcept {
        sink() = s;
    }

    void record(EventId id, util::u64 payload, ::trace::TraceKind kind) noexcept {
        auto& target = sink();
        if (target.emit) {
            target.emit(target.ctx, kind, static_cast<util::u32>(id), payload);
        }
    }
}

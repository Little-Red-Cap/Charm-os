module;

#include <cstdint>

export module kernel.evt;

import util.core;
import util.variant;

export namespace kernel {
    enum class EventId : std::uint16_t {
        init,
        tick,
        sync,
        message,
        terminate,
        user0,
        user1
    };

    using EventPayload = util::variant<util::monostate, util::u32, util::u64>;

    struct Event {
        EventId id{};
        EventPayload payload{};
    };

    inline Event make_event(EventId id) {
        return Event{id, util::monostate{}};
    }

    inline Event make_event(EventId id, util::u32 value) {
        return Event{id, value};
    }

    inline Event make_event(EventId id, util::u64 value) {
        return Event{id, value};
    }

    inline util::u32 payload_u32(const Event& evt, util::u32 fallback = 0) {
        if (auto value = util::get_if<util::u32>(&evt.payload)) {
            return *value;
        }
        return fallback;
    }

    inline util::u64 payload_u64(const Event& evt, util::u64 fallback = 0) {
        if (auto value = util::get_if<util::u64>(&evt.payload)) {
            return *value;
        }
        return fallback;
    }

    inline util::usize payload_usize(const Event& evt, util::usize fallback = 0) {
        return static_cast<util::usize>(payload_u64(evt, static_cast<util::u64>(fallback)));
    }
}

export module kernel.evt;

import <cstdint>;

export namespace kernel {
    enum class EventId : std::uint16_t {
        init,
        tick,
        user0,
        user1
    };

    struct Event {
        EventId id{};
        std::uint32_t value{0};
    };
}

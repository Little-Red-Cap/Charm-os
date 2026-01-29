module;

#include <cstddef>
#include <compare>

export module kernel.event_token;

import util.core;

export namespace kernel {
    struct EventToken {
        util::u64 value{0};
        constexpr auto operator<=>(const EventToken&) const = default;
    };
}

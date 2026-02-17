module;

#include <cstddef>
#include <compare>

export module kernel.wait_token;

import util.core;

export namespace kernel {
    struct WaitToken {
        util::u64 value{0};
        constexpr auto operator<=>(const WaitToken&) const = default;
    };

    struct CancelToken {
        util::u64 value{0};
        constexpr auto operator<=>(const CancelToken&) const = default;
    };
}

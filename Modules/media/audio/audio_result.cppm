module;

#include <cstdint>

export module audio.result;

import util.expected;

export namespace audio {
    enum class Errc : std::uint16_t {
        ok = 0,
        invalid_arg,
        not_supported,
        io_error,
        decode_error,
        bad_state,
        timeout
    };

    struct Err {
        Errc code{Errc::ok};
        std::uint16_t ext{0};
    };

    template <typename T>
    using Result = util::expected<T, Err>;

    using util::unexpected;
}

module;

#include <cstdint>
#include <expected>

export module audio.result;

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
    using Result = std::expected<T, Err>;
}

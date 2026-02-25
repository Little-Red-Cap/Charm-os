module;

#include <cstddef>

export module media.stream.types;

import util.core;
import util.expected;

export namespace media {
    enum class StreamKind : util::u8 {
        audio,
        video,
        other
    };

    struct StreamFormat {
        StreamKind kind{StreamKind::other};
        util::u32 rate{0};
        util::u16 channels{0};
        util::u16 bits_per_sample{0};
        util::u32 width{0};
        util::u32 height{0};
    };

    enum class Errc : util::u16 {
        ok = 0,
        not_supported,
        invalid_arg,
        io_error,
        decode_error,
        end_of_stream,
        bad_state,
        timeout
    };

    struct Error {
        Errc code{Errc::ok};
        util::u16 ext{0};
    };

    template <class T>
    using Result = util::expected<T, Error>;
}

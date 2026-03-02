module;

#include <cstddef>

export module media.stream.types;

import util.core;
import util.error;

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

    typedef util::Errc Errc;

    template <class T>
    using Result = util::Result<T>;
}

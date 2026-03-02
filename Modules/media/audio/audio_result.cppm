module;

#include <cstdint>

export module audio.result;

import util.error;
import util.expected;
import media.stream.types;

export namespace audio {
    template <typename T>
    using Result = media::Result<T>;

    typedef media::Errc Errc;

    using util::unexpected;
}

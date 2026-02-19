module;

#include <cstdint>

export module audio.result;

import util.expected;
import media.stream.types;

export namespace audio {
    template <typename T>
    using Result = media::Result<T>;

    using Errc = media::Errc;
    using Err = media::Error;

    using util::unexpected;
}

module;
#include <cstddef>
#include <cstring>
#include <span>

export module audio.sink.common;

import media.stream.sink;

export namespace audio {
    using FillCallback = media::FillCallback;

    struct FillStats {
        std::size_t filled{0};
        bool underrun{false};
    };

    inline FillStats fill_and_pad(FillCallback cb, void* user, std::span<std::byte> dst) noexcept {
        FillStats out{};
        if (cb) {
            out.filled = cb(dst, user);
        }
        if (out.filled < dst.size()) {
            std::memset(dst.data() + out.filled, 0, dst.size() - out.filled);
            out.underrun = true;
        }
        return out;
    }
}

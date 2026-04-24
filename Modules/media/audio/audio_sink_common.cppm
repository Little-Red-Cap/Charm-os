module;
#include <cstddef>
#include <cstdint>
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

    struct PullSpec {
        std::uint32_t period_frames{0};
        std::size_t period_bytes{0};
    };

    // Realtime/ISR pull contract: callback must be non-blocking and must not
    // call decoder/graph/storage or allocate memory.
    inline PullSpec resolve_pull_spec(const media::SinkConfig& cfg,
                                      std::size_t frame_size) noexcept {
        PullSpec out{};
        const std::uint32_t rate = cfg.format.rate;
        out.period_frames = cfg.period_frames != 0 ? cfg.period_frames : (rate / 100);
        out.period_bytes = static_cast<std::size_t>(out.period_frames) * frame_size;
        return out;
    }

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

module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module audio.channel.convert;

import audio.result;

export namespace audio {
    enum class ChannelMode : std::uint8_t {
        pass_through,
        upmix_1_to_2,
        downmix_2_to_1
    };

    struct ChannelConvertConfig {
        std::uint16_t in_ch{0};
        std::uint16_t out_ch{0};
    };

    class ChannelConverterS32 {
    public:
        Result<void> init(ChannelConvertConfig cfg) {
            if (cfg.in_ch == 0 || cfg.out_ch == 0) {
                return unexpected(Err{Errc::invalid_arg, 0});
            }
            in_ch_ = cfg.in_ch;
            out_ch_ = cfg.out_ch;

            if (in_ch_ == out_ch_) {
                mode_ = ChannelMode::pass_through;
                return {};
            }
            if (in_ch_ == 1 && out_ch_ == 2) {
                mode_ = ChannelMode::upmix_1_to_2;
                return {};
            }
            if (in_ch_ == 2 && out_ch_ == 1) {
                mode_ = ChannelMode::downmix_2_to_1;
                return {};
            }
            return unexpected(Err{Errc::not_supported, 0});
        }

        void reset() noexcept {}

        ChannelMode mode() const noexcept { return mode_; }
        std::uint16_t in_channels() const noexcept { return in_ch_; }
        std::uint16_t out_channels() const noexcept { return out_ch_; }

        std::size_t process(std::span<const std::int32_t> in,
                            std::span<std::int32_t> out) noexcept {
            if (in_ch_ == 0 || out_ch_ == 0) return 0;
            const std::size_t in_frames = in.size() / in_ch_;
            const std::size_t out_cap = out.size() / out_ch_;
            const std::size_t frames = std::min(in_frames, out_cap);

            if (frames == 0) return 0;

            if (mode_ == ChannelMode::pass_through) {
                const std::size_t samples = frames * in_ch_;
                std::memcpy(out.data(), in.data(), samples * sizeof(std::int32_t));
                return frames;
            }
            if (mode_ == ChannelMode::upmix_1_to_2) {
                for (std::size_t i = 0; i < frames; ++i) {
                    const std::int32_t v = in[i];
                    out[i * 2 + 0] = v;
                    out[i * 2 + 1] = v;
                }
                return frames;
            }
            if (mode_ == ChannelMode::downmix_2_to_1) {
                for (std::size_t i = 0; i < frames; ++i) {
                    const std::int32_t l = in[i * 2 + 0];
                    const std::int32_t r = in[i * 2 + 1];
                    const std::int64_t s = static_cast<std::int64_t>(l) + static_cast<std::int64_t>(r);
                    const std::int32_t y = static_cast<std::int32_t>(s / 2);
                    out[i] = y;
                }
                return frames;
            }
            return 0;
        }

    private:
        ChannelMode mode_{ChannelMode::pass_through};
        std::uint16_t in_ch_{0};
        std::uint16_t out_ch_{0};
    };
}

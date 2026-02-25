module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

export module audio.resampler.linear;

export namespace audio {
    constexpr std::size_t kResamplerMaxChannels = 2;

    struct ResampleResult {
        std::size_t in_used{0};
        std::size_t out_frames{0};
    };

    class LinearResamplerS32 {
    public:
        void configure(std::uint32_t src_rate, std::uint32_t dst_rate, std::uint16_t channels) {
            src_rate_ = src_rate;
            dst_rate_ = dst_rate;
            channels_ = channels;
            if (channels_ > kResamplerMaxChannels) {
                channels_ = 0;
            }
            if (dst_rate_ == 0) {
                step_ = 0;
            } else {
                step_ = (static_cast<std::uint64_t>(src_rate_) << 32) / dst_rate_;
            }
            reset();
        }

        void reset() {
            phase_ = 0;
            have_carry_ = false;
            for (std::size_t c = 0; c < kResamplerMaxChannels; ++c) {
                carry_[c] = 0;
            }
        }

        ResampleResult process(std::span<const std::int32_t> in, std::size_t in_frames,
                               std::span<std::int32_t> out, std::size_t out_frames_cap) {
            ResampleResult res{};
            if (channels_ == 0 || src_rate_ == 0 || dst_rate_ == 0) return res;

            const std::size_t channels = channels_;
            const std::size_t max_in_frames = in.size() / channels;
            in_frames = std::min(in_frames, max_in_frames);

            std::size_t in_used = 0;
            if (!have_carry_) {
                if (in_frames == 0) return res;
                for (std::size_t c = 0; c < channels; ++c) {
                    carry_[c] = in[c];
                }
                have_carry_ = true;
                in_used = 1;
                in = in.subspan(channels);
                in_frames -= 1;
            }

            const std::size_t virtual_frames = in_frames + 1;
            if (virtual_frames < 2) {
                res.in_used = in_used;
                res.out_frames = 0;
                return res;
            }

            auto frame_at = [&](std::size_t idx, std::size_t ch) -> std::int32_t {
                if (idx == 0) return carry_[ch];
                return in[(idx - 1) * channels + ch];
            };

            std::size_t out_frames = 0;
            while (out_frames < out_frames_cap) {
                const std::size_t pos = static_cast<std::size_t>(phase_ >> 32);
                if (pos + 1 >= virtual_frames) break;
                const std::uint64_t frac = phase_ & 0xFFFFFFFFull;

                for (std::size_t ch = 0; ch < channels; ++ch) {
                    const std::int32_t s0 = frame_at(pos, ch);
                    const std::int32_t s1 = frame_at(pos + 1, ch);
                    const std::int64_t diff = static_cast<std::int64_t>(s1) - s0;
                    const std::int64_t interp = static_cast<std::int64_t>(s0)
                        + ((diff * static_cast<std::int64_t>(frac)) >> 32);
                    out[out_frames * channels + ch] = static_cast<std::int32_t>(interp);
                }

                phase_ += step_;
                out_frames++;
            }

            std::size_t advance = static_cast<std::size_t>(phase_ >> 32);
            const std::size_t max_advance = virtual_frames - 1;
            if (advance > max_advance) advance = max_advance;

            if (advance > 0) {
                for (std::size_t ch = 0; ch < channels; ++ch) {
                    carry_[ch] = frame_at(advance, ch);
                }
                phase_ -= (static_cast<std::uint64_t>(advance) << 32);
                const std::size_t additional = std::min(advance, in_frames);
                in_used += additional;
            }

            res.in_used = in_used;
            res.out_frames = out_frames;
            return res;
        }

    private:
        std::uint32_t src_rate_{0};
        std::uint32_t dst_rate_{0};
        std::uint16_t channels_{0};
        std::uint64_t step_{0};
        std::uint64_t phase_{0};
        bool have_carry_{false};
        std::array<std::int32_t, kResamplerMaxChannels> carry_{};
    };
}

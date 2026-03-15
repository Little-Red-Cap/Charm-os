module;

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

export module audio.dsp_graph;

export namespace audio {
    class DspGraph {
    public:
        void reset(std::uint16_t channels) noexcept {
            channels_ = channels;
            fade_total_frames_ = 0;
            fade_remaining_frames_ = 0;
        }

        void set_gain(float gain) noexcept {
            gain_ = std::clamp(gain, 0.0f, 1.0f);
        }

        void reset_fade(std::uint64_t frames) noexcept {
            fade_total_frames_ = frames;
            fade_remaining_frames_ = frames;
        }

        void process(std::span<std::int32_t> samples, std::size_t frames) noexcept {
            if (channels_ == 0 || frames == 0) return;
            const std::size_t total_samples = frames * channels_;
            if (samples.size() < total_samples) {
                frames = samples.size() / channels_;
            }
            if (frames == 0) return;

            const std::uint64_t fade_total = fade_total_frames_;
            const std::uint64_t fade_remaining_start = fade_remaining_frames_;
            const bool use_fade = fade_total > 0 && fade_remaining_start > 0;
            const bool use_gain = gain_ != 1.0f;

            const std::int64_t min_v = std::numeric_limits<std::int32_t>::min();
            const std::int64_t max_v = std::numeric_limits<std::int32_t>::max();

            std::size_t sample_index = 0;
            for (std::size_t frame = 0; frame < frames; ++frame) {
                std::int64_t scale_num = 1;
                std::int64_t scale_den = 1;
                if (use_fade) {
                    const std::uint64_t done = fade_total - fade_remaining_start + frame + 1;
                    const std::uint64_t scale = std::min(done, fade_total);
                    scale_num = static_cast<std::int64_t>(scale);
                    scale_den = static_cast<std::int64_t>(fade_total == 0 ? 1 : fade_total);
                }

                for (std::size_t ch = 0; ch < channels_; ++ch, ++sample_index) {
                    std::int64_t v = samples[sample_index];
                    if (use_fade) {
                        v = (v * scale_num) / scale_den;
                    }
                    if (use_gain) {
                        v = static_cast<std::int64_t>(static_cast<float>(v) * gain_);
                    }
                    v = std::clamp(v, min_v, max_v);
                    samples[sample_index] = static_cast<std::int32_t>(v);
                }
            }

            if (use_fade) {
                fade_remaining_frames_ = (frames >= fade_remaining_start)
                    ? 0
                    : (fade_remaining_start - frames);
            }
        }

    private:
        std::uint16_t channels_{0};
        float gain_{1.0f};
        std::uint64_t fade_total_frames_{0};
        std::uint64_t fade_remaining_frames_{0};
    };
}

module;

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

export module audio.tone;

import audio.format;
import audio.fifo;
import audio.pcm_buffer;

export namespace audio {
    class SineTone {
    public:
        void set_freq_hz(float hz) noexcept { freq_hz_ = hz; }
        void set_gain(float gain) noexcept { gain_ = std::clamp(gain, 0.0f, 1.0f); }

        std::size_t render(std::span<std::byte> dst, const AudioFormat& fmt) noexcept {
            if (fmt.sample_type != SampleType::s16 || !fmt.interleaved || fmt.channels == 0 || fmt.rate == 0) {
                return 0;
            }
            const std::size_t frame_size = fmt.frame_size();
            if (frame_size == 0) return 0;
            const std::size_t frames = dst.size() / frame_size;
            if (frames == 0) return 0;

            auto* out = reinterpret_cast<std::int16_t*>(dst.data());
            const float step = static_cast<float>(2.0 * 3.14159265358979323846) * freq_hz_ /
                static_cast<float>(fmt.rate);
            const std::size_t channels = fmt.channels;
            for (std::size_t i = 0; i < frames; ++i) {
                const float v = std::sin(phase_) * gain_;
                phase_ += step;
                if (phase_ >= static_cast<float>(2.0 * 3.14159265358979323846)) {
                    phase_ -= static_cast<float>(2.0 * 3.14159265358979323846);
                }
                const auto s = static_cast<std::int16_t>(std::clamp(v, -1.0f, 1.0f) * 32767.0f);
                const std::size_t base = i * channels;
                for (std::size_t ch = 0; ch < channels; ++ch) {
                    out[base + ch] = s;
                }
            }
            return frames * frame_size;
        }

    private:
        float freq_hz_{440.0f};
        float gain_{0.2f};
        float phase_{0.0f};
    };

    inline std::size_t write_tone_fifo(
        PcmFifo& fifo,
        SineTone& tone,
        const AudioFormat& fmt,
        std::span<std::byte> scratch) noexcept {
        const std::size_t produced = tone.render(scratch, fmt);
        if (produced == 0) return 0;
        return write_pcm_fifo(fifo, scratch.first(produced), fmt.frame_size());
    }
}

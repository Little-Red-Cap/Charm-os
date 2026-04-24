module;

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module audio.decode_pipe;

import audio.channel.convert;
import audio.decoder.flac;
import audio.decoder.mp3;
import audio.decoder.wav;
import audio.eq;
import audio.format;
import audio.resampler.linear;
import audio.result;
import media.stream.filter;
import media.stream.source;
import media.stream.types;

export namespace audio {
    enum class SourceKind : std::uint8_t {
        wav,
        mp3,
        flac
    };

#ifndef CHARM_AUDIO_MAX_RATE
#define CHARM_AUDIO_MAX_RATE 48000
#endif
#ifndef CHARM_AUDIO_MAX_CHANNELS
#define CHARM_AUDIO_MAX_CHANNELS 2
#endif
#ifndef CHARM_AUDIO_MAX_PERIOD_FRAMES
#define CHARM_AUDIO_MAX_PERIOD_FRAMES 480
#endif
#ifndef CHARM_AUDIO_MAX_CHUNK_MULT
#define CHARM_AUDIO_MAX_CHUNK_MULT 10
#endif

    namespace detail {
        template <typename T, std::size_t Capacity>
        class StaticBuffer {
        public:
            bool resize(std::size_t n) noexcept {
                if (n > Capacity) return false;
                size_ = n;
                return true;
            }

            [[nodiscard]] std::size_t size() const noexcept { return size_; }
            [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }
            [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

            [[nodiscard]] T* data() noexcept { return data_.data(); }
            [[nodiscard]] const T* data() const noexcept { return data_.data(); }
            T& operator[](std::size_t idx) noexcept { return data_[idx]; }
            const T& operator[](std::size_t idx) const noexcept { return data_[idx]; }

        private:
            std::array<T, Capacity> data_{};
            std::size_t size_{0};
        };
    }

    class AudioDecodePipe {
    public:
        Result<void> open(media::StreamSourceRef src, SourceKind kind) noexcept {
            close();
            src_ = src;
            kind_ = kind;
            last_decode_eos_ = false;
            has_more_data_ = false;
            total_frames_ = 0;

            if (kind_ == SourceKind::flac) {
                const auto info = flac_filter_.open(src_);
                if (!info) return unexpected(info.error());
                const auto fmt = flac_filter_.format();
                input_fmt_.rate = fmt.rate;
                input_fmt_.channels = fmt.channels;
                input_fmt_.sample_type = SampleType::s16;
                has_more_data_ = true;
                total_frames_ = flac_filter_.total_frames();
            } else if (kind_ == SourceKind::mp3) {
                const auto info = mp3_filter_.open(src_);
                if (!info) return unexpected(info.error());
                const auto fmt = mp3_filter_.format();
                input_fmt_.rate = fmt.rate;
                input_fmt_.channels = fmt.channels;
                input_fmt_.sample_type = SampleType::s16;
                has_more_data_ = true;
                total_frames_ = mp3_filter_.total_frames();
            } else {
                const auto info = wav_filter_.open(src_);
                if (!info) return unexpected(info.error());
                const auto fmt = wav_filter_.format();
                if (fmt.bits_per_sample != 16) {
                    return unexpected(Errc::not_supported);
                }
                input_fmt_.rate = fmt.rate;
                input_fmt_.channels = fmt.channels;
                input_fmt_.sample_type = SampleType::s16;
                data_offset_ = wav_filter_.data_offset();
                data_size_ = wav_filter_.data_size();
                remaining_bytes_ = data_size_;
                has_more_data_ = remaining_bytes_ > 0;
                total_frames_ = data_size_ / input_fmt_.frame_size();
            }

            output_fmt_ = input_fmt_;
            return {};
        }

        void close() noexcept {
            flac_filter_.close();
            mp3_filter_.close();
            wav_filter_.close();
            src_ = {};
            kind_ = SourceKind::wav;
            has_more_data_ = false;
            last_decode_eos_ = false;
            total_frames_ = 0;
            data_offset_ = 0;
            data_size_ = 0;
            remaining_bytes_ = 0;
            resample_cache_frames_ = 0;
        }

        Result<void> seek_frames(std::uint64_t frames) noexcept {
            if (!src_.ops) return unexpected(Errc::bad_state);
            std::uint64_t clamped_frames = (total_frames_ > 0) ? std::min(frames, total_frames_) : frames;
            if (total_frames_ > 0 && clamped_frames >= total_frames_) {
                clamped_frames = total_frames_ - 1;
            }
            if (kind_ == SourceKind::wav) {
                const std::uint64_t offset = clamped_frames * input_fmt_.frame_size();
                const std::uint64_t clamped = std::min<std::uint64_t>(offset, data_size_);
                remaining_bytes_ = static_cast<std::size_t>(data_size_ - clamped);
                has_more_data_ = remaining_bytes_ > 0;
                auto res = src_.seek(static_cast<std::int64_t>(data_offset_ + clamped), media::SeekWhence::set);
                if (!res) return unexpected(res.error());
            } else if (kind_ == SourceKind::flac) {
                auto res = flac_filter_.seek_pcm_frame(clamped_frames);
                if (!res) return unexpected(res.error());
                has_more_data_ = true;
            } else if (kind_ == SourceKind::mp3) {
                auto res = mp3_filter_.seek_pcm_frame(clamped_frames);
                if (!res) return unexpected(res.error());
                has_more_data_ = true;
            } else {
                return unexpected(Errc::not_supported);
            }

            last_decode_eos_ = false;
            resample_cache_frames_ = 0;
            resampler_.reset();
            return {};
        }

        bool configure_output(std::uint16_t force_channels,
                              bool fixed_rate,
                              std::uint32_t fixed_rate_value) noexcept {
            output_fmt_ = input_fmt_;
            if (force_channels != 0) {
                output_fmt_.channels = force_channels;
            }

            if (!configure_channel_convert()) {
                return false;
            }

            resample_enabled_ = false;
            if (fixed_rate && fixed_rate_value > 0) {
                output_fmt_.rate = fixed_rate_value;
                if (output_fmt_.rate != input_fmt_.rate) {
                    resampler_.configure(input_fmt_.rate, output_fmt_.rate, output_fmt_.channels);
                    resampler_.reset();
                    resample_cache_frames_ = 0;
                    resample_enabled_ = true;
                }
            }

            if (output_fmt_.rate != 0) {
                update_eq_filters();
            }
            return true;
        }

        bool configure_buffers(std::uint32_t chunk_frames, std::size_t chunk_bytes) noexcept {
            chunk_frames_ = chunk_frames;
            chunk_bytes_ = chunk_bytes;
            const std::size_t out_samples = static_cast<std::size_t>(chunk_frames_) * output_fmt_.channels;
            if (!s32_out_.resize(out_samples)) return false;

            if (resample_enabled_) {
                input_chunk_frames_ = static_cast<std::size_t>(
                    (static_cast<std::uint64_t>(chunk_frames_) * input_fmt_.rate) / output_fmt_.rate) + 1;
                if (input_chunk_frames_ < 2) input_chunk_frames_ = 2;
                if (input_chunk_frames_ > kMaxInputFrames) return false;
                const std::size_t in_samples = input_chunk_frames_ * input_fmt_.channels;
                const std::size_t conv_samples = input_chunk_frames_ * output_fmt_.channels;
                if (!raw_.resize(in_samples * sizeof(std::int16_t))) return false;
                if (!s16_in_.resize(in_samples)) return false;
                if (!s32_in_.resize(in_samples)) return false;
                if (!s32_conv_.resize(conv_samples)) return false;
                if (!s32_work_.resize(conv_samples + output_fmt_.channels)) return false;
                if (!resample_cache_.resize(conv_samples + output_fmt_.channels)) return false;
            } else {
                input_chunk_frames_ = chunk_frames_;
                if (input_chunk_frames_ > kMaxInputFrames) return false;
                const std::size_t in_samples = input_chunk_frames_ * input_fmt_.channels;
                const std::size_t conv_samples = input_chunk_frames_ * output_fmt_.channels;
                if (!raw_.resize(in_samples * sizeof(std::int16_t))) return false;
                if (!s16_in_.resize(in_samples)) return false;
                if (!s32_in_.resize(in_samples)) return false;
                if (!s32_conv_.resize(conv_samples)) return false;
            }
            return true;
        }

        void set_eq(const EqConfig& eq) noexcept {
            eq_ = eq;
            if (eq_.band_count > EqConfig::max_bands) {
                eq_.band_count = EqConfig::max_bands;
            }
            eq_dirty_ = true;
        }

        void maybe_update_eq() noexcept {
            if (eq_dirty_ && output_fmt_.rate != 0) {
                update_eq_filters();
            }
        }

        std::size_t decode_frames(std::size_t frames_needed) noexcept {
            last_output_frames_ = 0;
            if (!has_more_data_) return 0;
            if (frames_needed == 0) return 0;

            const std::size_t input_target = resample_enabled_ ? input_chunk_frames_ : frames_needed;
            std::size_t decoded_frames = 0;

            if (kind_ == SourceKind::flac) {
                decoded_frames = read_flac(input_target);
            } else if (kind_ == SourceKind::mp3) {
                decoded_frames = read_mp3(input_target);
            } else {
                decoded_frames = read_wav(input_target);
            }

            if (decoded_frames == 0) {
                if (last_decode_eos_ && (!resample_enabled_ || resample_cache_frames_ == 0)) {
                    has_more_data_ = false;
                }
            }

            const std::size_t conv_frames = convert_channels(decoded_frames);
            std::size_t out_frames = 0;
            if (resample_enabled_) {
                out_frames = resample(conv_frames, frames_needed);
                if (out_frames > 0) {
                    apply_eq_s32(out_frames);
                }
            } else {
                if (conv_frames > 0) {
                    const std::size_t samples = conv_frames * output_fmt_.channels;
                    std::memcpy(s32_out_.data(), s32_conv_.data(), samples * sizeof(std::int32_t));
                    out_frames = conv_frames;
                    apply_eq_s32(out_frames);
                }
            }

            last_output_frames_ = out_frames;
            return out_frames;
        }

        std::span<const std::int32_t> output_frames() const noexcept {
            if (last_output_frames_ == 0 || output_fmt_.channels == 0) return {};
            const std::size_t samples = last_output_frames_ * output_fmt_.channels;
            return std::span<const std::int32_t>(s32_out_.data(), samples);
        }

        bool has_more_data() const noexcept { return has_more_data_; }
        std::uint64_t total_frames() const noexcept { return total_frames_; }
        const AudioFormat& input_format() const noexcept { return input_fmt_; }
        const AudioFormat& output_format() const noexcept { return output_fmt_; }

    private:
        static constexpr std::size_t kMaxRate = CHARM_AUDIO_MAX_RATE;
        static constexpr std::size_t kMaxChannels = CHARM_AUDIO_MAX_CHANNELS;
        static constexpr std::size_t kMaxPeriodFrames = CHARM_AUDIO_MAX_PERIOD_FRAMES;
        static constexpr std::size_t kMaxChunkMult = CHARM_AUDIO_MAX_CHUNK_MULT;
        static constexpr std::size_t kMaxChunkFrames = kMaxPeriodFrames * kMaxChunkMult;
        static constexpr std::size_t kMaxInputFrames = kMaxChunkFrames + 2;

        struct Biquad {
            float b0{1.0f};
            float b1{0.0f};
            float b2{0.0f};
            float a1{0.0f};
            float a2{0.0f};
            std::array<float, kMaxChannels> z1{};
            std::array<float, kMaxChannels> z2{};
            bool enabled{false};

            void reset() noexcept {
                z1.fill(0.0f);
                z2.fill(0.0f);
            }

            float process(float x, std::size_t ch) noexcept {
                if (!enabled || ch >= z1.size()) return x;
                const float y = b0 * x + z1[ch];
                z1[ch] = b1 * x + z2[ch] - a1 * y;
                z2[ch] = b2 * x - a2 * y;
                return y;
            }
        };

        std::size_t read_flac(std::size_t frames) {
            if (frames == 0) return 0;
            const std::size_t frame_bytes = input_fmt_.channels * sizeof(std::int32_t);
            const std::size_t out_frames = std::min(frames, s32_in_.size() / input_fmt_.channels);
            const std::size_t out_bytes = out_frames * frame_bytes;
            auto res = flac_filter_.process(std::span<const std::byte>{},
                std::span<std::byte>(reinterpret_cast<std::byte*>(s32_in_.data()), out_bytes));
            if (!res) return 0;
            last_decode_eos_ = res->end_of_stream;
            const std::size_t produced = res->produced - (res->produced % frame_bytes);
            return produced / frame_bytes;
        }

        std::size_t read_wav(std::size_t frames) {
            if (frames == 0) return 0;
            const std::size_t bytes_per_frame = input_fmt_.frame_size();
            const std::size_t max_bytes = std::min(frames * bytes_per_frame, raw_.size());
            auto res = wav_filter_.process(std::span<const std::byte>{},
                std::span<std::byte>(raw_.data(), max_bytes));
            if (!res) return 0;
            last_decode_eos_ = res->end_of_stream;
            const std::size_t produced = res->produced - (res->produced % bytes_per_frame);
            if (produced == 0) {
                if (last_decode_eos_) {
                    remaining_bytes_ = 0;
                }
                return 0;
            }
            if (remaining_bytes_ > 0) {
                remaining_bytes_ = (produced >= remaining_bytes_) ? 0 : (remaining_bytes_ - produced);
            }

            const std::size_t samples = produced / sizeof(std::int16_t);
            std::memcpy(s16_in_.data(), raw_.data(), samples * sizeof(std::int16_t));
            for (std::size_t i = 0; i < samples; ++i) {
                s32_in_[i] = static_cast<std::int32_t>(s16_in_[i]) << 16;
            }
            return samples / input_fmt_.channels;
        }

        std::size_t read_mp3(std::size_t frames) {
            if (frames == 0) return 0;
            const std::size_t frame_bytes = input_fmt_.channels * sizeof(std::int16_t);
            const std::size_t out_frames = std::min(frames, s16_in_.size() / input_fmt_.channels);
            const std::size_t out_bytes = out_frames * frame_bytes;
            auto res = mp3_filter_.process(std::span<const std::byte>{},
                std::span<std::byte>(reinterpret_cast<std::byte*>(s16_in_.data()), out_bytes));
            if (!res) return 0;
            last_decode_eos_ = res->end_of_stream;
            const std::size_t produced = res->produced - (res->produced % frame_bytes);
            if (produced == 0) {
                return 0;
            }
            const std::size_t read_samples = produced / sizeof(std::int16_t);
            for (std::size_t i = 0; i < read_samples; ++i) {
                s32_in_[i] = static_cast<std::int32_t>(s16_in_[i]) << 16;
            }
            return produced / frame_bytes;
        }

        std::size_t convert_channels(std::size_t frames) {
            if (frames == 0) return 0;
            const std::size_t out_samples = frames * output_fmt_.channels;
            if (out_samples > s32_conv_.size()) {
                if (!s32_conv_.resize(out_samples)) {
                    has_more_data_ = false;
                    return 0;
                }
            }
            const auto res = channel_conv_.process(
                std::span<const std::int32_t>(s32_in_.data(), frames * input_fmt_.channels),
                std::span<std::int32_t>(s32_conv_.data(), out_samples));
            return res;
        }

        bool configure_channel_convert() {
            const ChannelConvertConfig cfg{
                input_fmt_.channels,
                output_fmt_.channels
            };
            auto res = channel_conv_.init(cfg);
            return res.has_value();
        }

        std::size_t resample(std::size_t conv_frames, std::size_t out_frames_cap) {
            const std::size_t channels = output_fmt_.channels;
            std::size_t total_frames = resample_cache_frames_ + conv_frames;

            if (total_frames == 0) return 0;

            if (!s32_work_.resize((total_frames + 1) * channels)) {
                resample_cache_frames_ = 0;
                total_frames = conv_frames;
                if (!s32_work_.resize((total_frames + 1) * channels)) {
                    has_more_data_ = false;
                    return 0;
                }
            }
            if (resample_cache_frames_ > 0) {
                std::memcpy(
                    s32_work_.data(),
                    resample_cache_.data(),
                    resample_cache_frames_ * channels * sizeof(std::int32_t));
            }
            if (conv_frames > 0) {
                std::memcpy(
                    s32_work_.data() + (resample_cache_frames_ * channels),
                    s32_conv_.data(),
                    conv_frames * channels * sizeof(std::int32_t));
            }

            auto result = resampler_.process(
                std::span<const std::int32_t>(s32_work_.data(), total_frames * channels),
                total_frames,
                std::span<std::int32_t>(s32_out_.data(), out_frames_cap * channels),
                out_frames_cap);

            const std::size_t used = std::min(result.in_used, total_frames);
            const std::size_t remaining = total_frames - used;
            if (remaining > 0) {
                if (!resample_cache_.resize(remaining * channels)) {
                    has_more_data_ = false;
                    return 0;
                }
                std::memcpy(
                    resample_cache_.data(),
                    s32_work_.data() + (used * channels),
                    remaining * channels * sizeof(std::int32_t));
            }
            resample_cache_frames_ = remaining;
            return result.out_frames;
        }

        void update_eq_filters() {
            eq_dirty_ = false;
            for (auto& biquad : eq_biquads_) {
                biquad.enabled = false;
                biquad.reset();
            }
            if (!eq_.enabled || eq_.band_count == 0 || output_fmt_.rate == 0) {
                eq_ready_ = false;
                return;
            }
            const float fs = static_cast<float>(output_fmt_.rate);
            const float nyquist = fs * 0.5f - 1.0f;
            const float min_freq = 10.0f;
            for (std::size_t i = 0; i < eq_.band_count && i < eq_biquads_.size(); ++i) {
                const auto& band = eq_.bands[i];
                if (std::abs(band.gain_db) < 0.01f) {
                    continue;
                }
                const float freq = std::clamp(static_cast<float>(band.freq_hz), min_freq, nyquist);
                const float q = (band.q > 0.05f) ? band.q : 0.707f;
                const float a = std::pow(10.0f, band.gain_db / 40.0f);
                const float w0 = static_cast<float>(2.0 * 3.14159265358979323846) * freq / fs;
                const float cosw = std::cos(w0);
                const float sinw = std::sin(w0);
                const float alpha = sinw / (2.0f * q);

                const float b0 = 1.0f + alpha * a;
                const float b1 = -2.0f * cosw;
                const float b2 = 1.0f - alpha * a;
                const float a0 = 1.0f + alpha / a;
                const float a1 = -2.0f * cosw;
                const float a2 = 1.0f - alpha / a;

                auto& biquad = eq_biquads_[i];
                const float inv_a0 = (a0 != 0.0f) ? (1.0f / a0) : 1.0f;
                biquad.b0 = b0 * inv_a0;
                biquad.b1 = b1 * inv_a0;
                biquad.b2 = b2 * inv_a0;
                biquad.a1 = a1 * inv_a0;
                biquad.a2 = a2 * inv_a0;
                biquad.enabled = true;
            }
            eq_ready_ = true;
        }

        void apply_eq_s32(std::size_t frames) {
            if (!eq_ready_ || !eq_.enabled || eq_.band_count == 0) return;
            const std::size_t channels = output_fmt_.channels;
            if (channels == 0) return;
            constexpr float kInvScale = 2147483648.0f;
            constexpr float kScale = 1.0f / kInvScale;
            constexpr float kClamp = 32767.0f / 32768.0f;
            const std::size_t samples = frames * channels;
            for (std::size_t i = 0; i < samples; ++i) {
                const std::size_t ch = i % channels;
                float v = static_cast<float>(s32_out_[i]) * kScale;
                for (std::size_t b = 0; b < eq_.band_count && b < eq_biquads_.size(); ++b) {
                    v = eq_biquads_[b].process(v, ch);
                }
                v = std::clamp(v, -1.0f, kClamp);
                s32_out_[i] = static_cast<std::int32_t>(v * kInvScale);
            }
        }

        media::StreamSourceRef src_{};
        SourceKind kind_{SourceKind::wav};
        FlacFilter flac_filter_{};
        Mp3Filter mp3_filter_{};
        WavFilter wav_filter_{};

        AudioFormat input_fmt_{};
        AudioFormat output_fmt_{};
        bool resample_enabled_{false};
        std::size_t resample_cache_frames_{0};
        std::size_t input_chunk_frames_{0};
        std::uint32_t chunk_frames_{0};
        std::size_t chunk_bytes_{0};
        std::size_t last_output_frames_{0};

        std::size_t data_offset_{0};
        std::size_t data_size_{0};
        std::size_t remaining_bytes_{0};
        std::uint64_t total_frames_{0};
        bool has_more_data_{false};
        bool last_decode_eos_{false};

        EqConfig eq_{};
        std::array<Biquad, EqConfig::max_bands> eq_biquads_{};
        bool eq_ready_{false};
        bool eq_dirty_{false};

        detail::StaticBuffer<std::byte, kMaxInputFrames * kMaxChannels * sizeof(std::int16_t)> raw_{};
        detail::StaticBuffer<std::int16_t, kMaxInputFrames * kMaxChannels> s16_in_{};
        detail::StaticBuffer<std::int32_t, kMaxInputFrames * kMaxChannels> s32_in_{};
        detail::StaticBuffer<std::int32_t, kMaxChunkFrames * kMaxChannels> s32_out_{};
        detail::StaticBuffer<std::int32_t, kMaxInputFrames * kMaxChannels> s32_conv_{};
        static constexpr std::size_t kResampleCacheSlack = 16;
        detail::StaticBuffer<std::int32_t, (kMaxInputFrames + kResampleCacheSlack) * kMaxChannels> s32_work_{};
        detail::StaticBuffer<std::int32_t, (kMaxInputFrames + kResampleCacheSlack) * kMaxChannels> resample_cache_{};

        LinearResamplerS32 resampler_{};
        ChannelConverterS32 channel_conv_{};
    };
}

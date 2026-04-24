module;

#include <span>
#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

export module audio.sink.sdl3;

import audio.format;
import audio.result;
import audio.sink.common;
import charm.system.clock;
import media.stream.sink;
import media.stream.types;
export namespace audio {
#ifndef CHARM_AUDIO_MAX_PERIOD_FRAMES
#define CHARM_AUDIO_MAX_PERIOD_FRAMES 480
#endif
#ifndef CHARM_AUDIO_MAX_CHANNELS
#define CHARM_AUDIO_MAX_CHANNELS 2
#endif

    constexpr std::size_t kMaxScratchBytes =
        static_cast<std::size_t>(CHARM_AUDIO_MAX_PERIOD_FRAMES) *
        static_cast<std::size_t>(CHARM_AUDIO_MAX_CHANNELS) *
        sizeof(std::int16_t);

    using FillCallback = media::FillCallback;
    using SinkConfig = media::SinkConfig;

    struct CallbackStats {
        std::uint64_t count{0};
        std::uint64_t dt_min_ns{0};
        std::uint64_t dt_max_ns{0};
        double dt_avg_ms{0.0};
        std::uint32_t last_request_frames{0};
    };

    class Sdl3AudioSink {
    public:
        void set_clock(charm::system::Clock& clock) noexcept {
            clock_.reset(clock);
        }

        Result<void> open(const SinkConfig& cfg) noexcept {
            fmt_ = from_stream_format(cfg.format);
            const auto pull = resolve_pull_spec(cfg, fmt_.frame_size());
            callback_bytes_ = pull.period_bytes;
            period_frames_ = pull.period_frames;
            if (callback_bytes_ > scratch_.size()) {
                return unexpected(Errc::invalid_arg);
            }
            scratch_size_ = callback_bytes_;

            if (!SDL_Init(SDL_INIT_AUDIO)) {
                return unexpected(Errc::io_error);
            }

            spec_.freq = static_cast<int>(fmt_.rate);
            spec_.format = SDL_AUDIO_S16;
            spec_.channels = static_cast<int>(fmt_.channels);

            stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec_, &Sdl3AudioSink::sdl_audio_callback, this);
            if (!stream_) {
                return unexpected(Errc::io_error);
            }

            if (spec_.freq > 0) {
                latency_sec_ = static_cast<double>(period_frames_) / static_cast<double>(spec_.freq);
            }

            cb_dt_min_ns_.store(0, std::memory_order_relaxed);
            cb_dt_max_ns_.store(0, std::memory_order_relaxed);
            cb_dt_sum_ns_.store(0, std::memory_order_relaxed);
            cb_count_.store(0, std::memory_order_relaxed);
            cb_last_ns_.store(0, std::memory_order_relaxed);
            cb_last_request_frames_.store(0, std::memory_order_relaxed);

            return {};
        }

        Result<void> start() noexcept {
            if (!stream_) return unexpected(Errc::io_error);
            SDL_ResumeAudioStreamDevice(stream_);
            return {};
        }

        Result<void> stop() noexcept {
            if (!stream_) return unexpected(Errc::io_error);
            SDL_PauseAudioStreamDevice(stream_);
            return {};
        }

        void close() noexcept {
            if (stream_) {
                SDL_DestroyAudioStream(stream_);
                stream_ = nullptr;
            }
        }

        void set_fill_callback(FillCallback cb, void* user) noexcept {
            fill_cb_ = cb;
            fill_user_ = user;
        }

        media::StreamFormat format() const noexcept { return to_stream_format(fmt_); }
        std::uint32_t actual_period_frames() const noexcept { return period_frames_; }
        double output_latency_seconds() const noexcept { return latency_sec_; }

        std::uint64_t underrun_count() const noexcept {
            return underrun_count_.load(std::memory_order_relaxed);
        }

        bool consume_underrun_flag() noexcept {
            return underrun_flag_.exchange(0, std::memory_order_acq_rel) != 0;
        }

        void clear_underrun_flag() noexcept {
            underrun_flag_.store(0, std::memory_order_relaxed);
        }

        CallbackStats callback_stats() const noexcept {
            CallbackStats out{};
            out.count = cb_count_.load(std::memory_order_relaxed);
            out.dt_min_ns = cb_dt_min_ns_.load(std::memory_order_relaxed);
            out.dt_max_ns = cb_dt_max_ns_.load(std::memory_order_relaxed);
            const auto sum = cb_dt_sum_ns_.load(std::memory_order_relaxed);
            if (out.count > 0) {
                out.dt_avg_ms = (static_cast<double>(sum) / static_cast<double>(out.count)) / 1e6;
            }
            out.last_request_frames = cb_last_request_frames_.load(std::memory_order_relaxed);
            return out;
        }

    private:
        std::uint64_t now_ns() const noexcept {
            const auto now_us = clock_.now_us();
            return static_cast<std::uint64_t>(now_us) * 1000u;
        }

        static void update_min(std::atomic<std::uint64_t>& dst, std::uint64_t value) {
            auto cur = dst.load(std::memory_order_relaxed);
            if (cur == 0 || value < cur) {
                while (!dst.compare_exchange_weak(cur, value, std::memory_order_relaxed)) {
                    if (cur != 0 && cur <= value) break;
                }
            }
        }

        static void update_max(std::atomic<std::uint64_t>& dst, std::uint64_t value) {
            auto cur = dst.load(std::memory_order_relaxed);
            if (value > cur) {
                while (!dst.compare_exchange_weak(cur, value, std::memory_order_relaxed)) {
                    if (cur >= value) break;
                }
            }
        }

        static void sdl_audio_callback(void* userdata, SDL_AudioStream* stream, int additional_amount, int) {
            auto* self = static_cast<Sdl3AudioSink*>(userdata);
            const auto now = self->now_ns();
            const auto last = self->cb_last_ns_.exchange(now, std::memory_order_relaxed);
            if (last != 0) {
                const auto dt = now - last;
                update_min(self->cb_dt_min_ns_, dt);
                update_max(self->cb_dt_max_ns_, dt);
                self->cb_dt_sum_ns_.fetch_add(dt, std::memory_order_relaxed);
            }
            self->cb_count_.fetch_add(1, std::memory_order_relaxed);

            std::size_t request = additional_amount > 0
                ? static_cast<std::size_t>(additional_amount)
                : self->callback_bytes_;

            if (request == 0) return;

            const std::size_t frames = request / self->fmt_.frame_size();
            self->cb_last_request_frames_.store(static_cast<std::uint32_t>(frames), std::memory_order_relaxed);

            std::size_t remaining = request;
            while (remaining > 0) {
                const std::size_t chunk = std::min(remaining, self->scratch_size_);
                if (chunk == 0) break;
                const auto res = fill_and_pad(
                    self->fill_cb_,
                    self->fill_user_,
                    std::span<std::byte>(self->scratch_.data(), chunk));
                if (res.underrun) {
                    self->underrun_flag_.store(1, std::memory_order_relaxed);
                    self->underrun_count_.fetch_add(1, std::memory_order_relaxed);
                }

                SDL_PutAudioStreamData(stream, self->scratch_.data(), static_cast<int>(chunk));
                remaining -= chunk;
            }
        }

        FillCallback fill_cb_{nullptr};
        void* fill_user_{nullptr};

        SDL_AudioStream* stream_{nullptr};
        SDL_AudioSpec spec_{};
        AudioFormat fmt_{};
        std::uint32_t period_frames_{0};
        double latency_sec_{0.0};
        std::size_t callback_bytes_{0};

        std::array<std::byte, kMaxScratchBytes> scratch_{};
        std::size_t scratch_size_{0};

        std::atomic<std::uint8_t> underrun_flag_{0};
        std::atomic<std::uint64_t> underrun_count_{0};

        std::atomic<std::uint64_t> cb_count_{0};
        std::atomic<std::uint64_t> cb_dt_min_ns_{0};
        std::atomic<std::uint64_t> cb_dt_max_ns_{0};
        std::atomic<std::uint64_t> cb_dt_sum_ns_{0};
        std::atomic<std::uint64_t> cb_last_ns_{0};
        std::atomic<std::uint32_t> cb_last_request_frames_{0};
        charm::system::ClockRef clock_{};

        static AudioFormat from_stream_format(const media::StreamFormat& fmt) {
            AudioFormat out{};
            out.rate = fmt.rate;
            out.channels = fmt.channels;
            out.sample_type = SampleType::s16;
            out.interleaved = true;
            return out;
        }

        static media::StreamFormat to_stream_format(const AudioFormat& fmt) {
            media::StreamFormat out{};
            out.kind = media::StreamKind::audio;
            out.rate = fmt.rate;
            out.channels = fmt.channels;
            out.bits_per_sample = 16;
            return out;
        }
    };
}

module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "i2s.h"

export module audio.sink.i2s;

import audio.format;
import audio.result;
import media.stream.sink;
import media.stream.types;

export namespace audio {
#ifndef CHARM_AUDIO_MAX_PERIOD_FRAMES
#define CHARM_AUDIO_MAX_PERIOD_FRAMES 480
#endif
#ifndef CHARM_AUDIO_MAX_CHANNELS
#define CHARM_AUDIO_MAX_CHANNELS 2
#endif

    constexpr std::size_t kMaxBufferBytes =
        static_cast<std::size_t>(CHARM_AUDIO_MAX_PERIOD_FRAMES) *
        static_cast<std::size_t>(CHARM_AUDIO_MAX_CHANNELS) *
        sizeof(std::int16_t) * 2;

    using FillCallback = media::FillCallback;
    using SinkConfig = media::SinkConfig;

    struct CallbackStats {
        std::uint64_t count{0};
        std::uint64_t dt_min_ns{0};
        std::uint64_t dt_max_ns{0};
        double dt_avg_ms{0.0};
        std::uint32_t last_request_frames{0};
    };

    class I2sAudioSink {
    public:
        static I2S_HandleTypeDef& i2s_handle() noexcept {
#if defined(STM32F407xx)
            return hi2s2;
#else
            return hi2s1;
#endif
        }

        Result<void> open(const SinkConfig& cfg) noexcept {
            fmt_ = from_stream_format(cfg.format);
            const std::uint32_t period = cfg.period_frames != 0
                ? cfg.period_frames
                : (fmt_.rate / 100);
            period_frames_ = period;
            period_bytes_ = static_cast<std::size_t>(period_frames_) * fmt_.frame_size();
            buffer_bytes_ = period_bytes_ * 2;
            if (buffer_bytes_ > buffer_.size()) {
                return unexpected(Err{Errc::invalid_arg, 0});
            }
            return {};
        }

        Result<void> start() noexcept {
            if (!fill_cb_) return unexpected(Err{Errc::bad_state, 0});
            active_ = this;
            underrun_flag_ = 0;
            underrun_count_ = 0;
            cb_count_ = 0;
            cb_last_request_frames_ = period_frames_;

            auto view = std::span<std::byte>(buffer_.data(), buffer_bytes_);
            fill_block(view.first(period_bytes_));
            fill_block(view.subspan(period_bytes_, period_bytes_));

            if (HAL_I2S_Transmit_DMA(&i2s_handle(),
                    reinterpret_cast<uint16_t*>(buffer_.data()),
                    static_cast<uint16_t>(buffer_bytes_ / 2)) != HAL_OK) {
                active_ = nullptr;
                return unexpected(Err{Errc::io_error, 0});
            }
            return {};
        }

        Result<void> stop() noexcept {
            HAL_I2S_DMAStop(&i2s_handle());
            active_ = nullptr;
            return {};
        }

        void close() noexcept {
            (void)stop();
        }

        void set_fill_callback(FillCallback cb, void* user) noexcept {
            fill_cb_ = cb;
            fill_user_ = user;
        }

        media::StreamFormat format() const noexcept { return to_stream_format(fmt_); }
        std::uint32_t actual_period_frames() const noexcept { return period_frames_; }

        std::uint64_t underrun_count() const noexcept { return underrun_count_; }
        bool consume_underrun_flag() noexcept {
            const auto v = underrun_flag_;
            underrun_flag_ = 0;
            return v != 0;
        }
        void clear_underrun_flag() noexcept { underrun_flag_ = 0; }

        CallbackStats callback_stats() const noexcept {
            CallbackStats out{};
            out.count = cb_count_;
            out.last_request_frames = cb_last_request_frames_;
            return out;
        }

        static void on_half() noexcept {
            if (active_) active_->fill_half(0);
        }

        static void on_full() noexcept {
            if (active_) active_->fill_half(1);
        }

        static void on_error() noexcept {
            if (active_) active_->underrun_flag_ = 1;
        }

    private:
        void fill_half(std::size_t half) noexcept {
            if (!fill_cb_) return;
            auto view = std::span<std::byte>(buffer_.data(), buffer_bytes_);
            auto block = view.subspan(half * period_bytes_, period_bytes_);
            fill_block(block);
        }

        void fill_block(std::span<std::byte> block) noexcept {
            std::size_t written = 0;
            if (fill_cb_) {
                written = fill_cb_(block, fill_user_);
            }
            if (written < block.size()) {
                std::memset(block.data() + written, 0, block.size() - written);
                underrun_flag_ = 1;
                ++underrun_count_;
            }
            ++cb_count_;
        }

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

        static inline I2sAudioSink* active_{nullptr};

        FillCallback fill_cb_{nullptr};
        void* fill_user_{nullptr};

        AudioFormat fmt_{};
        std::uint32_t period_frames_{0};
        std::size_t period_bytes_{0};
        std::size_t buffer_bytes_{0};

        std::array<std::byte, kMaxBufferBytes> buffer_{};

        std::uint8_t underrun_flag_{0};
        std::uint64_t underrun_count_{0};
        std::uint64_t cb_count_{0};
        std::uint32_t cb_last_request_frames_{0};
    };
}

extern "C" void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s == &audio::I2sAudioSink::i2s_handle()) {
        audio::I2sAudioSink::on_half();
    }
    extern void charm_audio_i2s_debug_toggle();
    charm_audio_i2s_debug_toggle();
    extern void charm_audio_i2s_half_notify();
    charm_audio_i2s_half_notify();
}

extern "C" void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s == &audio::I2sAudioSink::i2s_handle()) {
        audio::I2sAudioSink::on_full();
    }
    extern void charm_audio_i2s_debug_toggle();
    charm_audio_i2s_debug_toggle();
    extern void charm_audio_i2s_full_notify();
    charm_audio_i2s_full_notify();
}

extern "C" void HAL_I2S_ErrorCallback(I2S_HandleTypeDef* hi2s) {
    if (hi2s == &audio::I2sAudioSink::i2s_handle()) {
        audio::I2sAudioSink::on_error();
    }
}

extern "C" void charm_audio_i2s_debug_toggle() __attribute__((weak));
extern "C" void charm_audio_i2s_debug_toggle() {
}

extern "C" void charm_audio_i2s_half_notify() __attribute__((weak));
extern "C" void charm_audio_i2s_half_notify() {
}

extern "C" void charm_audio_i2s_full_notify() __attribute__((weak));
extern "C" void charm_audio_i2s_full_notify() {
}

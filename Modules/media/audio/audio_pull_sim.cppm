module;

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

export module audio.pull_sim;

import audio.format;
import audio.result;
import audio.sink.common;
import charm.system.clock;
import media.stream.sink;
import media.stream.types;

export namespace audio {
    using FillCallback = media::FillCallback;
    using SinkConfig = media::SinkConfig;

    struct PullSimStats {
        std::uint64_t callback_count{0};
        std::uint64_t dt_min_ns{0};
        std::uint64_t dt_max_ns{0};
        double dt_avg_ms{0.0};
        std::uint32_t last_request_frames{0};
    };

    enum class JitterPattern : std::uint8_t {
        uniform,
        burst
    };

    class AudioPullSimulator {
    public:
        void set_clock(charm::system::Clock& clock) noexcept { clock_.reset(clock); }
        void set_jitter_ms(std::uint32_t ms) noexcept {
            jitter_ms_ = ms;
            jitter_dist_ = std::uniform_int_distribution<std::uint32_t>(0, jitter_ms_);
        }
        void set_jitter_seed(std::uint32_t seed) noexcept {
            jitter_seed_ = seed;
            rng_.seed(seed);
        }
        void set_jitter_pattern(JitterPattern pattern) noexcept { jitter_pattern_ = pattern; }

        Result<void> open(const SinkConfig& cfg) noexcept {
            fmt_ = from_stream_format(cfg.format);
            if (fmt_.rate == 0 || fmt_.channels == 0) {
                return unexpected(Errc::invalid_arg);
            }
            const auto pull = resolve_pull_spec(cfg, fmt_.frame_size());
            period_frames_ = pull.period_frames;
            period_bytes_ = pull.period_bytes;
            if (period_bytes_ == 0) {
                return unexpected(Errc::invalid_arg);
            }
            buffer_.assign(period_bytes_, std::byte{0});
            reset_stats();
            if (jitter_seed_ == 0) {
                seed_rng();
            }
            return {};
        }

        Result<void> start() noexcept {
            if (!fill_cb_) return unexpected(Errc::bad_state);
            running_ = true;
            last_tick_ns_ = 0;
            return {};
        }

        Result<void> stop() noexcept {
            running_ = false;
            return {};
        }

        void close() noexcept {
            running_ = false;
            buffer_.clear();
        }

        void set_fill_callback(FillCallback cb, void* user) noexcept {
            fill_cb_ = cb;
            fill_user_ = user;
        }

        media::StreamFormat format() const noexcept { return to_stream_format(fmt_); }
        std::uint32_t actual_period_frames() const noexcept { return period_frames_; }

        std::uint64_t underrun_count() const noexcept {
            return underrun_count_.load(std::memory_order_relaxed);
        }

        bool consume_underrun_flag() noexcept {
            return underrun_flag_.exchange(0, std::memory_order_acq_rel) != 0;
        }

        void clear_underrun_flag() noexcept {
            underrun_flag_.store(0, std::memory_order_relaxed);
        }

        PullSimStats callback_stats() const noexcept {
            PullSimStats out{};
            out.callback_count = cb_count_.load(std::memory_order_relaxed);
            out.dt_min_ns = cb_dt_min_ns_.load(std::memory_order_relaxed);
            out.dt_max_ns = cb_dt_max_ns_.load(std::memory_order_relaxed);
            const auto sum = cb_dt_sum_ns_.load(std::memory_order_relaxed);
            if (out.callback_count > 0) {
                out.dt_avg_ms = (static_cast<double>(sum) / static_cast<double>(out.callback_count)) / 1e6;
            }
            out.last_request_frames = cb_last_request_frames_.load(std::memory_order_relaxed);
            return out;
        }

        void reset_stats() noexcept {
            cb_count_.store(0, std::memory_order_relaxed);
            cb_dt_min_ns_.store(0, std::memory_order_relaxed);
            cb_dt_max_ns_.store(0, std::memory_order_relaxed);
            cb_dt_sum_ns_.store(0, std::memory_order_relaxed);
            cb_last_request_frames_.store(0, std::memory_order_relaxed);
            underrun_flag_.store(0, std::memory_order_relaxed);
            underrun_count_.store(0, std::memory_order_relaxed);
        }

        std::uint64_t next_jitter_us() noexcept {
            if (jitter_ms_ == 0) return 0;
            if (jitter_pattern_ == JitterPattern::burst) {
                if (jitter_burst_remaining_ > 0) {
                    --jitter_burst_remaining_;
                    return static_cast<std::uint64_t>(jitter_ms_) * 1000u;
                }
                if (jitter_burst_gap_ > 0) {
                    --jitter_burst_gap_;
                    return 0;
                }
                jitter_burst_gap_ = jitter_dist_(rng_);
                jitter_burst_remaining_ = 1 + (jitter_ms_ / 2);
                return static_cast<std::uint64_t>(jitter_ms_) * 1000u;
            }
            return static_cast<std::uint64_t>(jitter_dist_(rng_)) * 1000u;
        }

        bool step_once() noexcept {
            if (!running_ || !fill_cb_ || buffer_.empty()) return false;

            const auto now = now_ns();
            if (last_tick_ns_ != 0 && now != 0) {
                const auto dt = now - last_tick_ns_;
                update_min(cb_dt_min_ns_, dt);
                update_max(cb_dt_max_ns_, dt);
                cb_dt_sum_ns_.fetch_add(dt, std::memory_order_relaxed);
            }
            last_tick_ns_ = now;
            cb_count_.fetch_add(1, std::memory_order_relaxed);

            const std::size_t frames = period_frames_;
            cb_last_request_frames_.store(static_cast<std::uint32_t>(frames), std::memory_order_relaxed);

            const auto res = fill_and_pad(
                fill_cb_,
                fill_user_,
                std::span<std::byte>(buffer_.data(), buffer_.size()));
            if (res.underrun) {
                underrun_flag_.store(1, std::memory_order_relaxed);
                underrun_count_.fetch_add(1, std::memory_order_relaxed);
            }
            return true;
        }

    private:
        std::uint64_t now_ns() const noexcept {
            const auto now_us = clock_.now_us();
            return static_cast<std::uint64_t>(now_us) * 1000u;
        }

        void seed_rng() noexcept {
            const auto seed = static_cast<std::uint32_t>(clock_.now_us());
            rng_.seed(seed);
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

        FillCallback fill_cb_{nullptr};
        void* fill_user_{nullptr};

        AudioFormat fmt_{};
        std::uint32_t period_frames_{0};
        std::size_t period_bytes_{0};
        std::vector<std::byte> buffer_{};
        bool running_{false};
        std::uint64_t last_tick_ns_{0};
        std::uint32_t jitter_ms_{0};
        std::uint32_t jitter_seed_{0};
        JitterPattern jitter_pattern_{JitterPattern::uniform};
        std::minstd_rand rng_{};
        std::uniform_int_distribution<std::uint32_t> jitter_dist_{0, 0};
        std::uint32_t jitter_burst_gap_{0};
        std::uint32_t jitter_burst_remaining_{0};

        std::atomic<std::uint8_t> underrun_flag_{0};
        std::atomic<std::uint64_t> underrun_count_{0};

        std::atomic<std::uint64_t> cb_count_{0};
        std::atomic<std::uint64_t> cb_dt_min_ns_{0};
        std::atomic<std::uint64_t> cb_dt_max_ns_{0};
        std::atomic<std::uint64_t> cb_dt_sum_ns_{0};
        std::atomic<std::uint32_t> cb_last_request_frames_{0};
        charm::system::ClockRef clock_{};
    };
}

module;

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <complex>
#include <cmath>
#include <span>

#ifndef CHARM_AUDIO_ENABLE_STRESS
#define CHARM_AUDIO_ENABLE_STRESS 1
#endif

#if CHARM_AUDIO_ENABLE_STRESS
#include <random>
#include <thread>
#endif

export module audio.player;

import audio.decoder.flac;
import audio.decoder.mp3;
import audio.decoder.wav;
import audio.channel.convert;
import audio.fifo;
import audio.format;
import audio.resampler.linear;
import audio.result;
import audio.sink.sdl3;
import alg_fft;
import service.queue;

#if defined(CHARM_AUDIO_USE_VFS)
import audio.source.fs;
#else
import audio.source.file;
#endif

export namespace audio {
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
#ifndef CHARM_AUDIO_MAX_FIFO_MS
#define CHARM_AUDIO_MAX_FIFO_MS 300
#endif
#ifndef CHARM_AUDIO_MAX_PATH
#define CHARM_AUDIO_MAX_PATH 260
#endif
#ifndef CHARM_AUDIO_CMD_QUEUE_CAP
#define CHARM_AUDIO_CMD_QUEUE_CAP 8
#endif

    constexpr std::size_t kMaxRate = CHARM_AUDIO_MAX_RATE;
    constexpr std::size_t kMaxChannels = CHARM_AUDIO_MAX_CHANNELS;
    constexpr std::size_t kMaxPeriodFrames = CHARM_AUDIO_MAX_PERIOD_FRAMES;
    constexpr std::size_t kMaxChunkMult = CHARM_AUDIO_MAX_CHUNK_MULT;
    constexpr std::size_t kMaxFifoMs = CHARM_AUDIO_MAX_FIFO_MS;
    constexpr std::size_t kMaxPath = CHARM_AUDIO_MAX_PATH;
    constexpr std::size_t kCommandQueueCap = CHARM_AUDIO_CMD_QUEUE_CAP;

    constexpr std::size_t kMaxChunkFrames = kMaxPeriodFrames * kMaxChunkMult;
    constexpr std::size_t kMaxInputFrames = kMaxChunkFrames + 2;
    constexpr std::size_t kMaxFifoBytes =
        (static_cast<std::size_t>(kMaxRate) * kMaxFifoMs / 1000) *
        (kMaxChannels * sizeof(std::int16_t));

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

    template <std::size_t Capacity>
    class FixedString {
    public:
        bool assign(const char* value) noexcept {
            if (!value) return false;
            const std::size_t len = std::strlen(value);
            if (len >= Capacity) return false;
            std::memcpy(buf_.data(), value, len);
            buf_[len] = '\0';
            size_ = len;
            return true;
        }

        [[nodiscard]] const char* c_str() const noexcept { return buf_.data(); }
        [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    private:
        std::array<char, Capacity> buf_{};
        std::size_t size_{0};
    };

    enum class OutputMode : std::uint8_t {
        follow_input,
        fixed_rate
    };

    enum class PlayerState : std::uint8_t {
        idle,
        opening,
        buffering,
        playing,
        paused,
        stopping,
        error
    };

    enum class PlayerErrorStage : std::uint8_t {
        none = 0,
        open_source,
        unsupported_format,
        decode_open,
        wav_parse,
        wav_bits,
        channel_convert,
        buffer_config,
        sink_open,
        buffer_alloc,
        sink_start,
        seek,
        resume,
        reconfigure
    };

    struct PlayerProfile {
        std::uint32_t low_ms{40};
        std::uint32_t high_ms{150};
        std::uint32_t chunk_mult{8};
        std::uint32_t fifo_ms{300};
    };

    struct PlayerConfig {
        PlayerProfile profile{};
        std::uint32_t preferred_period_frames{0};
        OutputMode output_mode{OutputMode::follow_input};
        std::uint32_t fixed_rate{0};
        std::uint32_t fade_in_ms{0};
        std::uint16_t force_channels{0};
        std::uint8_t fail_reconfig_step{0};
    };

    struct EqBand {
        std::uint32_t freq_hz{1000};
        float gain_db{0.0f};
        float q{1.0f};
    };

    struct EqConfig {
        static constexpr std::size_t max_bands = 8;
        bool enabled{false};
        std::uint8_t band_count{0};
        std::array<EqBand, max_bands> bands{};
    };

    struct PlayerStats {
        std::uint64_t underrun_count{0};
        std::size_t min_water{0};
        std::size_t max_water{0};
        std::uint64_t refill_count{0};
        std::uint64_t overrun_count{0};
        double refill_min_ms{0.0};
        double refill_max_ms{0.0};
        double refill_sum_ms{0.0};
    };

    struct PlayerSnapshot {
        PlayerStats stats{};
        CallbackStats callback{};
        AudioFormat input_fmt{};
        AudioFormat output_fmt{};
        std::size_t water_bytes{0};
        std::size_t low_water{0};
        std::size_t high_water{0};
        std::size_t fifo_capacity{0};
        std::uint32_t period_frames{0};
        std::uint32_t chunk_frames{0};
    };

    class AudioPlayer {
    public:
        explicit AudioPlayer(PlayerConfig config)
            : config_(config) {
            init_spectrum_window();
        }

        ~AudioPlayer() { stop_internal(); }

        Result<void> play(const char* path) {
            if (!path || !*path) return unexpected(Err{Errc::invalid_arg, 0});
            Command cmd{};
            cmd.type = CommandType::play;
            if (!cmd.path.assign(path)) {
                return unexpected(Err{Errc::invalid_arg, 0});
            }
            if (!queue_.push(cmd)) {
                return unexpected(Err{Errc::timeout, 0});
            }
            return {};
        }

        Result<void> stop() {
            Command cmd{};
            cmd.type = CommandType::stop;
            if (!queue_.push(cmd)) {
                return unexpected(Err{Errc::timeout, 0});
            }
            return {};
        }

        Result<void> pause() {
            Command cmd{};
            cmd.type = CommandType::pause;
            if (!queue_.push(cmd)) {
                return unexpected(Err{Errc::timeout, 0});
            }
            return {};
        }

        Result<void> resume() {
            Command cmd{};
            cmd.type = CommandType::resume;
            if (!queue_.push(cmd)) {
                return unexpected(Err{Errc::timeout, 0});
            }
            return {};
        }

        Result<void> set_eq(const EqConfig& eq) {
            Command cmd{};
            cmd.type = CommandType::set_eq;
            cmd.eq = eq;
            if (!queue_.push(cmd)) {
                return unexpected(Err{Errc::timeout, 0});
            }
            return {};
        }

        Result<void> seek_ms(std::uint64_t ms) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening) {
                return unexpected(Err{Errc::bad_state, 0});
            }
            if (!is_wav_ && !is_flac_ && !is_mp3_) {
                return unexpected(Err{Errc::not_supported, 0});
            }
            if (total_frames_ == 0) {
                return unexpected(Err{Errc::not_supported, 0});
            }
            Command cmd{};
            cmd.type = CommandType::seek_ms;
            cmd.seek_ms = ms;
            if (!queue_.push(cmd)) {
                return unexpected(Err{Errc::timeout, 0});
            }
            return {};
        }

        Result<void> reconfigure_format(const AudioFormat& input_fmt) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening) {
                return unexpected(Err{Errc::bad_state, 0});
            }
            Command cmd{};
            cmd.type = CommandType::reconfigure;
            cmd.fmt = input_fmt;
            if (!queue_.push(cmd)) {
                return unexpected(Err{Errc::timeout, 0});
            }
            return {};
        }

        Result<void> reconfigure_output(std::uint32_t fixed_rate, std::uint32_t fade_in_ms) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening) {
                return unexpected(Err{Errc::bad_state, 0});
            }
            config_.fade_in_ms = fade_in_ms;
            if (fixed_rate == 0) {
                config_.output_mode = OutputMode::follow_input;
                config_.fixed_rate = 0;
            } else {
                config_.output_mode = OutputMode::fixed_rate;
                config_.fixed_rate = fixed_rate;
            }
            return reconfigure_format(input_fmt_);
        }

        void set_stress_ms(std::uint32_t ms) {
            stress_ms_ = ms;
#if CHARM_AUDIO_ENABLE_STRESS
            stress_dist_ = std::uniform_int_distribution<int>(0, static_cast<int>(stress_ms_));
#else
            (void)stress_ms_;
#endif
        }

        void tick() {
            process_commands();

            if (state_ == PlayerState::idle || state_ == PlayerState::error || state_ == PlayerState::paused) {
                return;
            }

            const std::size_t water = fifo_capacity_ ? fifo_.size_bytes() : 0;
            stats_.min_water = std::min(stats_.min_water, water);
            stats_.max_water = std::max(stats_.max_water, water);

            if (sink_.consume_underrun_flag()) {
                stats_.underrun_count = sink_.underrun_count();
            }

            if (state_ == PlayerState::buffering) {
                while (fifo_capacity_ && fifo_.size_bytes() < high_water_) {
                    refill_once();
                    if (!running_) break;
                    if (!has_more_data_ && fifo_.size_bytes() == 0) break;
                }
                if (fifo_capacity_ && fifo_.size_bytes() >= high_water_) {
                    if (!sink_.start()) {
                        set_error(Errc::io_error, PlayerErrorStage::sink_start);
                        return;
                    }
                    state_ = PlayerState::playing;
                }
                return;
            }

            if (state_ == PlayerState::playing) {
                if (water <= low_water_ || stats_.underrun_count != last_underrun_seen_) {
                    last_underrun_seen_ = stats_.underrun_count;
#if CHARM_AUDIO_ENABLE_STRESS
                    if (stress_ms_ > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(stress_dist_(rng_)));
                    }
#endif
                    refill_once();
                }
            }

            if (!has_more_data_ && fifo_capacity_ && fifo_.size_bytes() == 0) {
                stop_internal();
            }
        }

        PlayerState state() const noexcept { return state_; }

        bool is_running() const noexcept {
            return state_ != PlayerState::idle && state_ != PlayerState::error;
        }

        std::uint64_t total_frames() const noexcept { return total_frames_; }

        AudioFormat input_format() const noexcept { return input_fmt_; }

        Err last_error() const noexcept { return last_err_; }

        EqConfig eq_config() const noexcept { return eq_; }

        static constexpr std::size_t spectrum_bins = 32;
        static constexpr std::size_t spectrum_fft_size = 256;

        void enable_spectrum(bool on) noexcept {
            spectrum_enabled_.store(on, std::memory_order_relaxed);
            if (!on) {
                spectrum_ready_.store(false, std::memory_order_relaxed);
            }
        }

        bool read_spectrum(std::span<float> out) const noexcept {
            if (!spectrum_ready_.load(std::memory_order_acquire)) return false;
            if (out.size() < spectrum_bins) return false;
            const std::uint32_t idx = spectrum_index_.load(std::memory_order_acquire) & 1u;
            for (std::size_t i = 0; i < spectrum_bins; ++i) {
                out[i] = spectrum_[idx][i];
            }
            return true;
        }

        PlayerSnapshot snapshot(bool reset_window) {
            PlayerSnapshot snap{};
            snap.stats = stats_;
            snap.callback = sink_.callback_stats();
            snap.input_fmt = input_fmt_;
            snap.output_fmt = output_fmt_;
            snap.water_bytes = fifo_capacity_ ? fifo_.size_bytes() : 0;
            snap.low_water = low_water_;
            snap.high_water = high_water_;
            snap.fifo_capacity = fifo_capacity_;
            snap.period_frames = period_frames_;
            snap.chunk_frames = chunk_frames_;

            if (reset_window) {
                stats_.min_water = fifo_capacity_;
                stats_.max_water = 0;
            }
            return snap;
        }

    private:
        enum class CommandType : std::uint8_t { play, stop, pause, resume, seek_ms, reconfigure, set_eq };

        struct Command {
            CommandType type{};
            FixedString<kMaxPath> path{};
            std::uint64_t seek_ms{0};
            AudioFormat fmt{};
            EqConfig eq{};
        };

        static std::size_t fill_from_fifo(std::span<std::byte> dst, void* user) noexcept {
            auto* self = static_cast<AudioPlayer*>(user);
            if (!self || self->fifo_capacity_ == 0) return 0;
            const std::size_t frame = self->output_fmt_.frame_size();
            if (frame == 0) return 0;

            std::size_t need = dst.size() - (dst.size() % frame);
            std::size_t filled = 0;

            while (filled < need) {
                auto v = self->fifo_.readable_view();
                if (v.a.empty() && v.b.empty()) break;

                auto copy_one = [&](std::span<std::byte> src) {
                    std::size_t n = std::min(src.size(), need - filled);
                    n -= n % frame;
                    if (n == 0) return;
                    std::memcpy(dst.data() + filled, src.data(), n);
                    self->fifo_.commit_read(n);
                    filled += n;
                };

                if (!v.a.empty()) copy_one(v.a);
                else if (!v.b.empty()) copy_one(v.b);
            }

            if (filled > 0) {
                self->push_spectrum_samples(dst.first(filled));
            }
            return filled;
        }

        void init_spectrum_window() noexcept {
            constexpr float kPi = 3.14159265358979323846f;
            for (std::size_t i = 0; i < spectrum_fft_size; ++i) {
                const float phase = static_cast<float>(i) / static_cast<float>(spectrum_fft_size - 1);
                spectrum_window_[i] = 0.5f - 0.5f * std::cos(phase * 2.0f * kPi);
            }
        }

        void push_spectrum_samples(std::span<const std::byte> data) noexcept {
            if (!spectrum_enabled_.load(std::memory_order_relaxed)) return;
            const std::size_t channels = output_fmt_.channels ? output_fmt_.channels : 1;
            if (channels == 0) return;
            const auto* samples = reinterpret_cast<const std::int16_t*>(data.data());
            const std::size_t sample_count = data.size() / sizeof(std::int16_t);
            const std::size_t frames = sample_count / channels;
            for (std::size_t i = 0; i < frames; ++i) {
                const std::int16_t s = samples[i * channels];
                spectrum_time_[spectrum_pos_++] = static_cast<float>(s) / 32768.0f;
                if (spectrum_pos_ >= spectrum_fft_size) {
                    spectrum_pos_ = 0;
                    compute_spectrum();
                }
            }
        }

        void compute_spectrum() noexcept {
            for (std::size_t i = 0; i < spectrum_fft_size; ++i) {
                spectrum_fft_[i] = std::complex<double>(
                    static_cast<double>(spectrum_time_[i] * spectrum_window_[i]), 0.0);
            }
            alg::fft_inplace<spectrum_fft_size>(spectrum_fft_, false);

            constexpr std::size_t half = spectrum_fft_size / 2;
            constexpr std::size_t band = (half / spectrum_bins) > 0 ? (half / spectrum_bins) : 1;

            std::array<float, spectrum_bins> out{};
            for (std::size_t b = 0; b < spectrum_bins; ++b) {
                const std::size_t start = b * band;
                const std::size_t end = std::min(half, start + band);
                double sum = 0.0;
                std::size_t count = 0;
                for (std::size_t i = start; i < end; ++i) {
                    sum += std::abs(spectrum_fft_[i]);
                    ++count;
                }
                const double avg = (count > 0) ? (sum / static_cast<double>(count)) : 0.0;
                float norm = static_cast<float>(std::log10(1.0 + avg) / 3.0);
                if (norm < 0.0f) norm = 0.0f;
                if (norm > 1.0f) norm = 1.0f;
                out[b] = norm;
            }

            const std::uint32_t next = (spectrum_index_.load(std::memory_order_relaxed) + 1u) & 1u;
            spectrum_[next] = out;
            spectrum_index_.store(next, std::memory_order_release);
            spectrum_ready_.store(true, std::memory_order_release);
        }

        void process_commands() {
            for (;;) {
                auto cmd = queue_.pop();
                if (!cmd) break;
                if (cmd->type == CommandType::play) {
                    handle_play(cmd->path.c_str());
                } else if (cmd->type == CommandType::stop) {
                    stop_internal();
                } else if (cmd->type == CommandType::pause) {
                    handle_pause();
                } else if (cmd->type == CommandType::resume) {
                    handle_resume();
                } else if (cmd->type == CommandType::seek_ms) {
                    handle_seek(cmd->seek_ms);
                } else if (cmd->type == CommandType::reconfigure) {
                    handle_reconfigure(cmd->fmt);
                } else if (cmd->type == CommandType::set_eq) {
                    eq_ = cmd->eq;
                    if (eq_.band_count > EqConfig::max_bands) {
                        eq_.band_count = EqConfig::max_bands;
                    }
                }
            }
        }

        void handle_play(const char* path) {
            stop_internal();
            state_ = PlayerState::opening;
            last_err_ = Err{};

            if (!src_.open(path)) {
                set_error(Errc::io_error, PlayerErrorStage::open_source);
                return;
            }

            is_flac_ = ends_with_icase(path, ".flac") || ends_with_icase(path, ".fla");
            is_wav_ = ends_with_icase(path, ".wav");
            is_mp3_ = ends_with_icase(path, ".mp3");

            if (!is_flac_ && !is_wav_ && !is_mp3_) {
                set_error(Errc::not_supported, PlayerErrorStage::unsupported_format);
                return;
            }

            if (is_flac_) {
                const auto info = flac_.open(src_);
                if (!info) {
                    set_error(Errc::decode_error, PlayerErrorStage::decode_open);
                    return;
                }
                input_fmt_.rate = info->sample_rate;
                input_fmt_.channels = info->channels;
                input_fmt_.sample_type = SampleType::s16;
                has_more_data_ = true;
                total_frames_ = flac_.total_frames();
            } else if (is_mp3_) {
                const auto info = mp3_.open(src_);
                if (!info) {
                    set_error(Errc::decode_error, PlayerErrorStage::decode_open);
                    return;
                }
                input_fmt_.rate = info->sample_rate;
                input_fmt_.channels = info->channels;
                input_fmt_.sample_type = SampleType::s16;
                has_more_data_ = true;
                total_frames_ = mp3_.total_frames();
            } else {
                const auto info = parse_wav(src_);
                if (!info) {
                    set_error(Errc::decode_error, PlayerErrorStage::wav_parse);
                    return;
                }
                if (info->bits_per_sample != 16) {
                    set_error(Errc::not_supported, PlayerErrorStage::wav_bits);
                    return;
                }
                input_fmt_.rate = info->sample_rate;
                input_fmt_.channels = info->channels;
                input_fmt_.sample_type = SampleType::s16;
                data_offset_ = info->data_offset;
                data_size_ = info->data_size;
                remaining_bytes_ = info->data_size;
                has_more_data_ = remaining_bytes_ > 0;
                total_frames_ = data_size_ / input_fmt_.frame_size();
            }

            output_fmt_ = input_fmt_;
            if (config_.force_channels != 0) {
                output_fmt_.channels = config_.force_channels;
            }

            if (!configure_channel_convert()) {
                set_error(Errc::bad_state, PlayerErrorStage::channel_convert);
                return;
            }
            resample_enabled_ = false;
            if (config_.output_mode == OutputMode::fixed_rate && config_.fixed_rate > 0) {
                output_fmt_.rate = config_.fixed_rate;
                if (output_fmt_.rate != input_fmt_.rate) {
                    resampler_.configure(input_fmt_.rate, output_fmt_.rate, output_fmt_.channels);
                    resampler_.reset();
                    resample_cache_frames_ = 0;
                    resample_enabled_ = true;
                }
            }

            if (!configure_buffers()) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_config);
                return;
            }

            SinkConfig cfg{};
            cfg.fmt = output_fmt_;
            cfg.preferred_period_frames = config_.preferred_period_frames;
            if (!sink_.open(cfg)) {
                set_error(Errc::io_error, PlayerErrorStage::sink_open);
                return;
            }
            period_frames_ = sink_.actual_period_frames();
            if (period_frames_ == 0) {
                period_frames_ = output_fmt_.rate / 100;
            }
            chunk_frames_ = period_frames_ * config_.profile.chunk_mult;
            chunk_bytes_ = static_cast<std::size_t>(chunk_frames_) * output_fmt_.frame_size();
            if (!allocate_buffers(chunk_bytes_)) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_alloc);
                return;
            }

            sink_.set_fill_callback(&AudioPlayer::fill_from_fifo, this);

            stats_ = {};
            stats_.min_water = fifo_capacity_;
            stats_.max_water = 0;
            last_underrun_seen_ = 0;
            fade_in_remaining_frames_ = fade_in_total_frames();

            if (is_wav_) {
                auto seek = src_.seek(static_cast<std::int64_t>(data_offset_), SEEK_SET);
                if (!seek) {
                    set_error(Errc::io_error, PlayerErrorStage::seek);
                    return;
                }
            }

            running_ = true;
            state_ = PlayerState::buffering;
        }

        void handle_seek(std::uint64_t ms) {
            if (state_ == PlayerState::idle) return;
            (void)sink_.stop();
            if (fifo_capacity_) fifo_.clear();
            const std::uint64_t frames = (static_cast<std::uint64_t>(input_fmt_.rate) * ms) / 1000;
            std::uint64_t clamped_frames = (total_frames_ > 0) ? std::min(frames, total_frames_) : frames;
            if (total_frames_ > 0 && clamped_frames >= total_frames_) {
                clamped_frames = total_frames_ - 1;
            }
            if (is_wav_) {
                const std::uint64_t offset = clamped_frames * input_fmt_.frame_size();
                const std::uint64_t clamped = std::min<std::uint64_t>(offset, data_size_);
                remaining_bytes_ = static_cast<std::size_t>(data_size_ - clamped);
                has_more_data_ = remaining_bytes_ > 0;
                auto res = src_.seek(static_cast<std::int64_t>(data_offset_ + clamped), SEEK_SET);
                if (!res) {
                    set_error(Errc::io_error, PlayerErrorStage::seek);
                    return;
                }
            } else if (is_flac_) {
                auto reset = src_.seek(0, SEEK_SET);
                if (!reset) {
                    set_error(Errc::io_error, PlayerErrorStage::seek);
                    running_ = false;
                    return;
                }
                flac_.close();
                auto info = flac_.open(src_);
                if (!info) {
                    set_error(Errc::decode_error, PlayerErrorStage::seek);
                    running_ = false;
                    return;
                }
                input_fmt_.rate = info->sample_rate;
                input_fmt_.channels = info->channels;
                input_fmt_.sample_type = SampleType::s16;
                total_frames_ = flac_.total_frames();
                has_more_data_ = true;

                const std::size_t max_frames = s32_in_.size() / input_fmt_.channels;
                if (max_frames == 0) {
                    set_error(Errc::bad_state, PlayerErrorStage::seek);
                    running_ = false;
                    return;
                }
                std::uint64_t remaining = clamped_frames;
                while (remaining > 0) {
                    const std::size_t chunk = static_cast<std::size_t>(
                        std::min<std::uint64_t>(remaining, max_frames));
                    auto read = flac_.read_s32(s32_in_.data(), chunk);
                    if (!read || *read == 0) {
                        set_error(Errc::decode_error, PlayerErrorStage::seek);
                        running_ = false;
                        return;
                    }
                    remaining -= *read;
                }
            } else if (is_mp3_) {
                auto res = mp3_.seek_pcm_frame(clamped_frames);
                if (!res) {
                    set_error(Errc::io_error, PlayerErrorStage::seek);
                    running_ = false;
                    return;
                }
                has_more_data_ = true;
            } else {
                return;
            }
            stats_.min_water = fifo_capacity_;
            stats_.max_water = 0;
            if (resample_enabled_) {
                resampler_.reset();
                resample_cache_frames_ = 0;
            }
            fade_in_remaining_frames_ = fade_in_total_frames();
            state_ = PlayerState::buffering;
        }

        void handle_pause() {
            if (state_ == PlayerState::playing || state_ == PlayerState::buffering) {
                (void)sink_.stop();
                running_ = false;
                state_ = PlayerState::paused;
            }
        }

        void handle_resume() {
            if (state_ != PlayerState::paused) return;
            running_ = true;
            state_ = PlayerState::buffering;
            if (fifo_capacity_ && fifo_.size_bytes() >= high_water_) {
                if (!sink_.start()) {
                    set_error(Errc::io_error, PlayerErrorStage::resume);
                    return;
                }
                state_ = PlayerState::playing;
            }
        }

        void handle_reconfigure(const AudioFormat& input_fmt) {
            if (input_fmt.channels == 0 || input_fmt.rate == 0) {
                set_error(Errc::bad_state, PlayerErrorStage::reconfigure);
                return;
            }
            (void)sink_.stop();
            sink_.clear_underrun_flag();
            sink_.close();
            if (fifo_capacity_) fifo_.clear();

            input_fmt_ = input_fmt;
            output_fmt_ = input_fmt_;
            if (config_.force_channels != 0) {
                output_fmt_.channels = config_.force_channels;
            }
            if (!configure_channel_convert()) {
                set_error(Errc::bad_state, PlayerErrorStage::channel_convert);
                return;
            }
            resample_enabled_ = false;
            if (config_.output_mode == OutputMode::fixed_rate && config_.fixed_rate > 0) {
                output_fmt_.rate = config_.fixed_rate;
                if (output_fmt_.rate != input_fmt_.rate) {
                    resampler_.configure(input_fmt_.rate, output_fmt_.rate, output_fmt_.channels);
                    resampler_.reset();
                    resample_cache_frames_ = 0;
                    resample_enabled_ = true;
                }
            }

            if (config_.fail_reconfig_step == 1) {
                set_error(Errc::bad_state, PlayerErrorStage::reconfigure);
                return;
            }

            if (!configure_buffers()) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_config);
                return;
            }

            SinkConfig cfg{};
            cfg.fmt = output_fmt_;
            cfg.preferred_period_frames = config_.preferred_period_frames;
            if (!sink_.open(cfg)) {
                set_error(Errc::io_error, PlayerErrorStage::sink_open);
                return;
            }
            period_frames_ = sink_.actual_period_frames();
            if (period_frames_ == 0) {
                period_frames_ = output_fmt_.rate / 100;
            }
            chunk_frames_ = period_frames_ * config_.profile.chunk_mult;
            chunk_bytes_ = static_cast<std::size_t>(chunk_frames_) * output_fmt_.frame_size();
            if (!allocate_buffers(chunk_bytes_)) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_alloc);
                return;
            }
            sink_.set_fill_callback(&AudioPlayer::fill_from_fifo, this);

            stats_.min_water = fifo_capacity_;
            stats_.max_water = 0;
            last_underrun_seen_ = sink_.underrun_count();
            fade_in_remaining_frames_ = fade_in_total_frames();
            state_ = PlayerState::buffering;
        }

        void stop_internal() {
            if (state_ == PlayerState::idle) return;
            state_ = PlayerState::stopping;
            (void)sink_.stop();
            sink_.close();
            flac_.close();
            mp3_.close();
            src_.close();
            if (fifo_capacity_) fifo_.clear();
            running_ = false;
            has_more_data_ = false;
            is_flac_ = false;
            is_wav_ = false;
            is_mp3_ = false;
            data_offset_ = 0;
            data_size_ = 0;
            remaining_bytes_ = 0;
            total_frames_ = 0;
            resample_enabled_ = false;
            resample_cache_frames_ = 0;
            last_err_ = Err{};
            spectrum_pos_ = 0;
            spectrum_ready_.store(false, std::memory_order_relaxed);
            state_ = PlayerState::idle;
        }

        void set_error(Errc code, PlayerErrorStage stage) noexcept {
            last_err_ = Err{code, static_cast<std::uint16_t>(stage)};
            state_ = PlayerState::error;
        }

        bool configure_buffers() {
            if (output_fmt_.rate > kMaxRate || input_fmt_.rate > kMaxRate) return false;
            if (output_fmt_.channels == 0 || output_fmt_.channels > kMaxChannels) return false;
            if (input_fmt_.channels == 0 || input_fmt_.channels > kMaxChannels) return false;
            if (config_.profile.chunk_mult == 0 || config_.profile.chunk_mult > kMaxChunkMult) return false;
            if (config_.profile.fifo_ms == 0 || config_.profile.fifo_ms > kMaxFifoMs) return false;
            fifo_capacity_ = ms_to_bytes(config_.profile.fifo_ms, output_fmt_);
            if (fifo_capacity_ > kMaxFifoBytes) return false;
            if (!fifo_storage_.resize(fifo_capacity_)) return false;
            fifo_.reset(std::span<std::byte>(fifo_storage_.data(), fifo_capacity_));
            low_water_ = ms_to_bytes(config_.profile.low_ms, output_fmt_);
            high_water_ = ms_to_bytes(config_.profile.high_ms, output_fmt_);
            period_frames_ = config_.preferred_period_frames != 0
                ? config_.preferred_period_frames
                : (output_fmt_.rate / 100);
            if (period_frames_ > kMaxPeriodFrames) return false;
            chunk_frames_ = period_frames_ * config_.profile.chunk_mult;
            chunk_bytes_ = static_cast<std::size_t>(chunk_frames_) * output_fmt_.frame_size();
            return allocate_buffers(chunk_bytes_);
        }

        bool allocate_buffers(std::size_t chunk_bytes) {
            const std::size_t out_samples = chunk_bytes / sizeof(std::int16_t);
            if (!s16_out_.resize(out_samples)) return false;
            if (!s32_out_.resize(out_samples)) return false;
            if (resample_enabled_) {
                input_chunk_frames_ = static_cast<std::size_t>(
                    (static_cast<std::uint64_t>(chunk_frames_) * input_fmt_.rate) / output_fmt_.rate) + 2;
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

        void refill_once() {
            if (!fifo_capacity_) return;
            if (!has_more_data_) return;
            std::size_t writable = std::min(fifo_.free_bytes(), chunk_bytes_);
            writable = (writable / output_fmt_.frame_size()) * output_fmt_.frame_size();
            if (writable == 0) {
                stats_.overrun_count++;
                return;
            }

            const auto t0 = std::chrono::steady_clock::now();
            std::size_t bytes_written = 0;

            const std::size_t frames_needed = writable / output_fmt_.frame_size();
            const std::size_t input_target = resample_enabled_ ? input_chunk_frames_ : frames_needed;
            std::size_t decoded_frames = 0;

            if (is_flac_) {
                decoded_frames = read_flac(input_target);
            } else if (is_mp3_) {
                decoded_frames = read_mp3(input_target);
            } else {
                decoded_frames = read_wav(input_target);
            }

            if (decoded_frames == 0) {
                if (!resample_enabled_) {
                    has_more_data_ = false;
                } else if (resample_cache_frames_ == 0) {
                    has_more_data_ = false;
                }
            }

            const std::size_t conv_frames = convert_channels(decoded_frames);
            if (resample_enabled_) {
                const std::size_t out_frames = resample(conv_frames, frames_needed);
                if (out_frames > 0) {
                    bytes_written = quantize_s32(out_frames);
                }
            } else {
                if (conv_frames > 0) {
                    const std::size_t samples = conv_frames * output_fmt_.channels;
                    std::memcpy(s32_out_.data(), s32_conv_.data(), samples * sizeof(std::int32_t));
                    bytes_written = quantize_s32(conv_frames);
                }
            }

            if (bytes_written > 0) {
            auto view = fifo_.writable_view();
            bytes_written = std::min(bytes_written, view.a.size() + view.b.size());

                std::size_t written = 0;
                const std::size_t a = std::min(view.a.size(), bytes_written);
                std::memcpy(view.a.data(), reinterpret_cast<std::byte*>(s16_out_.data()), a);
                written += a;

                if (written < bytes_written && !view.b.empty()) {
                    const std::size_t b = std::min(view.b.size(), bytes_written - written);
                    std::memcpy(view.b.data(), reinterpret_cast<std::byte*>(s16_out_.data()) + written, b);
                    written += b;
                }
                fifo_.commit_write(written);
            }

            const auto t1 = std::chrono::steady_clock::now();
            const double dt_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
            update_refill_stats(dt_ms);
        }

        std::size_t read_flac(std::size_t frames) {
            if (frames == 0) return 0;
            auto res = flac_.read_s32(s32_in_.data(), frames);
            if (!res) {
                state_ = PlayerState::error;
                running_ = false;
                return 0;
            }
            return *res;
        }

        std::size_t read_wav(std::size_t frames) {
            if (frames == 0) return 0;
            const std::size_t bytes_per_frame = input_fmt_.frame_size();
            const std::size_t to_read = std::min(frames * bytes_per_frame, remaining_bytes_);
            auto res = src_.read(std::span<std::byte>(raw_.data(), to_read));
            if (!res) {
                state_ = PlayerState::error;
                running_ = false;
                return 0;
            }
            if (*res == 0) {
                remaining_bytes_ = 0;
                return 0;
            }
            remaining_bytes_ -= *res;
            if (remaining_bytes_ == 0) {
                has_more_data_ = false;
            }

            const std::size_t samples = *res / sizeof(std::int16_t);
            std::memcpy(s16_in_.data(), raw_.data(), samples * sizeof(std::int16_t));
            for (std::size_t i = 0; i < samples; ++i) {
                s32_in_[i] = static_cast<std::int32_t>(s16_in_[i]) << 16;
            }
            return samples / input_fmt_.channels;
        }

        std::size_t read_mp3(std::size_t frames) {
            if (frames == 0) return 0;
            const std::size_t samples = frames * input_fmt_.channels;
            if (samples > s16_in_.size()) {
                if (!s16_in_.resize(samples)) {
                    state_ = PlayerState::error;
                    running_ = false;
                    return 0;
                }
                if (!s32_in_.resize(samples)) {
                    state_ = PlayerState::error;
                    running_ = false;
                    return 0;
                }
            }
            auto res = mp3_.read_s16(s16_in_.data(), frames);
            if (!res) {
                state_ = PlayerState::error;
                running_ = false;
                return 0;
            }
            if (*res == 0) {
                has_more_data_ = false;
                return 0;
            }
            const std::size_t read_samples = (*res) * input_fmt_.channels;
            for (std::size_t i = 0; i < read_samples; ++i) {
                s32_in_[i] = static_cast<std::int32_t>(s16_in_[i]) << 16;
            }
            return *res;
        }

        std::size_t convert_channels(std::size_t frames) {
            if (frames == 0) return 0;
            const std::size_t out_samples = frames * output_fmt_.channels;
            if (out_samples > s32_conv_.size()) {
                if (!s32_conv_.resize(out_samples)) {
                    state_ = PlayerState::error;
                    running_ = false;
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
            if (!res) return false;
            return true;
        }

        std::size_t resample(std::size_t conv_frames, std::size_t out_frames_cap) {
            const std::size_t channels = output_fmt_.channels;
            std::size_t total_frames = resample_cache_frames_ + conv_frames;

            if (total_frames == 0) return 0;

            if (!s32_work_.resize((total_frames + 1) * channels)) {
                state_ = PlayerState::error;
                running_ = false;
                return 0;
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
                    state_ = PlayerState::error;
                    running_ = false;
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

        std::size_t quantize_s32(std::size_t frames) {
            const std::size_t samples = frames * output_fmt_.channels;
            const std::uint64_t fade_total = fade_in_total_frames();
            const std::uint64_t fade_remaining = fade_in_remaining_frames_;
            for (std::size_t i = 0; i < samples; ++i) {
                std::int32_t v = s32_out_[i];
                if (fade_remaining > 0) {
                    const std::size_t frame_index = i / output_fmt_.channels;
                    const std::uint64_t done = fade_total - fade_remaining + frame_index + 1;
                    const std::uint64_t scale = fade_total == 0 ? 0 : std::min(done, fade_total);
                    const std::int64_t scaled = (static_cast<std::int64_t>(v) * static_cast<std::int64_t>(scale)) /
                        static_cast<std::int64_t>(fade_total == 0 ? 1 : fade_total);
                    v = static_cast<std::int32_t>(scaled);
                }
                const std::int32_t clamped = std::clamp(
                    v,
                    static_cast<std::int32_t>(-32768 << 16),
                    static_cast<std::int32_t>(32767 << 16));
                s16_out_[i] = static_cast<std::int16_t>(clamped >> 16);
            }
            if (fade_in_remaining_frames_ > 0) {
                fade_in_remaining_frames_ = (frames >= fade_in_remaining_frames_) ? 0 : (fade_in_remaining_frames_ - frames);
            }
            return samples * sizeof(std::int16_t);
        }

        std::uint64_t fade_in_total_frames() const {
            if (config_.fade_in_ms == 0) return 0;
            return (static_cast<std::uint64_t>(output_fmt_.rate) * config_.fade_in_ms) / 1000;
        }

        void update_refill_stats(double ms) {
            if (stats_.refill_count == 0) {
                stats_.refill_min_ms = ms;
                stats_.refill_max_ms = ms;
            } else {
                stats_.refill_min_ms = std::min(stats_.refill_min_ms, ms);
                stats_.refill_max_ms = std::max(stats_.refill_max_ms, ms);
            }
            stats_.refill_sum_ms += ms;
            stats_.refill_count++;
        }

        std::size_t ms_to_bytes(std::uint32_t ms, const AudioFormat& fmt) const {
            const std::uint64_t frames = (static_cast<std::uint64_t>(fmt.rate) * ms) / 1000;
            return static_cast<std::size_t>(frames * fmt.frame_size());
        }

        bool ends_with_icase(const char* text, const char* suffix) const {
            if (!text || !suffix) return false;
            const std::size_t value_len = std::strlen(text);
            const std::size_t suf_len = std::strlen(suffix);
            if (value_len < suf_len) return false;
            const std::size_t start = value_len - suf_len;
            for (std::size_t i = 0; i < suf_len; ++i) {
                const char a = static_cast<char>(std::tolower(text[start + i]));
                const char b = static_cast<char>(std::tolower(suffix[i]));
                if (a != b) return false;
            }
            return true;
        }

        PlayerConfig config_{};
        PlayerStats stats_{};
        PlayerState state_{PlayerState::idle};
        service::Queue<Command, kCommandQueueCap> queue_{};

#if defined(CHARM_AUDIO_USE_VFS)
        FsDataSource src_{};
#else
        FileDataSource src_{};
#endif
        FlacDecoder flac_{};
        Mp3Decoder mp3_{};
        Sdl3AudioSink sink_{};
        PcmFifo fifo_{};

        AudioFormat input_fmt_{};
        AudioFormat output_fmt_{};
        std::size_t fifo_capacity_{0};
        std::size_t low_water_{0};
        std::size_t high_water_{0};
        std::uint32_t period_frames_{0};
        std::uint32_t chunk_frames_{0};
        std::size_t chunk_bytes_{0};
        std::size_t input_chunk_frames_{0};

        StaticBuffer<std::byte, kMaxFifoBytes> fifo_storage_{};
        StaticBuffer<std::byte, kMaxInputFrames * kMaxChannels * sizeof(std::int16_t)> raw_{};
        StaticBuffer<std::int16_t, kMaxInputFrames * kMaxChannels> s16_in_{};
        StaticBuffer<std::int16_t, kMaxChunkFrames * kMaxChannels> s16_out_{};
        StaticBuffer<std::int32_t, kMaxInputFrames * kMaxChannels> s32_in_{};
        StaticBuffer<std::int32_t, kMaxChunkFrames * kMaxChannels> s32_out_{};
        StaticBuffer<std::int32_t, kMaxInputFrames * kMaxChannels> s32_conv_{};
        StaticBuffer<std::int32_t, (kMaxInputFrames + 1) * kMaxChannels> s32_work_{};
        StaticBuffer<std::int32_t, (kMaxInputFrames + 1) * kMaxChannels> resample_cache_{};
        std::size_t resample_cache_frames_{0};
        LinearResamplerS32 resampler_{};
        bool resample_enabled_{false};
        std::uint64_t fade_in_remaining_frames_{0};
        ChannelConverterS32 channel_conv_{};

        std::size_t data_offset_{0};
        std::size_t data_size_{0};
        std::size_t remaining_bytes_{0};
        std::uint64_t total_frames_{0};
        Err last_err_{};
        EqConfig eq_{};

        std::array<float, spectrum_fft_size> spectrum_time_{};
        std::array<float, spectrum_fft_size> spectrum_window_{};
        std::array<std::complex<double>, spectrum_fft_size> spectrum_fft_{};
        std::array<float, spectrum_bins> spectrum_[2]{};
        std::atomic<std::uint32_t> spectrum_index_{0};
        std::atomic<bool> spectrum_ready_{false};
        std::atomic<bool> spectrum_enabled_{false};
        std::size_t spectrum_pos_{0};

        bool is_flac_{false};
        bool is_wav_{false};
        bool is_mp3_{false};
        bool running_{false};
        bool has_more_data_{false};
        std::uint64_t last_underrun_seen_{0};

        std::uint32_t stress_ms_{0};
#if CHARM_AUDIO_ENABLE_STRESS
        std::mt19937 rng_{static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())};
        std::uniform_int_distribution<int> stress_dist_{0, 0};
#endif
    };
}

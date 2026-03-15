module;

#include <algorithm>
#include <array>
#include <atomic>
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
#endif

export module audio.player;

import audio.decoder.flac;
import audio.decoder.mp3;
import audio.decoder.wav;
import audio.channel.convert;
import audio.data_plane;
import audio.format;
import audio.resampler.linear;
import audio.result;
import alg_fft;
import charm.system.clock;
import media.stream.filter;
import media.stream.sink;
import media.stream.source;
import media.stream.types;
import service.queue;
#if defined(CHARM_ENABLE_SDL3)
import audio.sink.sdl3;
#endif
#if defined(CHARM_AUDIO_SINK_I2S)
import audio.sink.i2s;
#endif

#if defined(CHARM_AUDIO_USE_VFS)
import audio.source.fs;
#else
import audio.source.file;
#endif

export namespace audio {
#if !defined(CHARM_ENABLE_SDL3) && !defined(CHARM_AUDIO_SINK_I2S)
    using SinkConfig = media::SinkConfig;
    using FillCallback = media::FillCallback;

    struct CallbackStats {
        std::uint64_t count{0};
        std::uint64_t dt_min_ns{0};
        std::uint64_t dt_max_ns{0};
        double dt_avg_ms{0.0};
        std::uint32_t last_request_frames{0};
    };

    class NullAudioSink {
    public:
        void set_clock(charm::system::Clock&) noexcept {}
        Result<void> open(const SinkConfig&) noexcept { return {}; }
        Result<void> start() noexcept { return {}; }
        Result<void> stop() noexcept { return {}; }
        void close() noexcept {}
        void set_fill_callback(FillCallback, void*) noexcept {}
        media::StreamFormat format() const noexcept { return {}; }
        std::uint32_t actual_period_frames() const noexcept { return 0; }
        std::uint64_t underrun_count() const noexcept { return 0; }
        bool consume_underrun_flag() noexcept { return false; }
        void clear_underrun_flag() noexcept {}
        CallbackStats callback_stats() const noexcept { return {}; }
    };
#endif

#if defined(CHARM_AUDIO_SINK_I2S)
    using SinkType = I2sAudioSink;
#elif defined(CHARM_ENABLE_SDL3)
    using SinkType = Sdl3AudioSink;
#else
    using SinkType = NullAudioSink;
#endif
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
        AudioPlayer(PlayerConfig config, charm::system::Clock& clock)
            : config_(config) {
            set_clock(clock);
            init_spectrum_window();
        }

        ~AudioPlayer() { stop_internal(); }

        void set_clock(charm::system::Clock& clock) noexcept {
            clock_.reset(clock);
            sink_.set_clock(clock);
#if CHARM_AUDIO_ENABLE_STRESS
            seed_rng();
#endif
        }

        Result<void> play(const char* path) {
            if (!path || !*path) return unexpected(Errc::invalid_arg);
            Command cmd{};
            cmd.type = CommandType::play;
            if (!cmd.path.assign(path)) {
                return unexpected(Errc::invalid_arg);
            }
            if (!queue_.push(cmd)) {
                return unexpected(Errc::timeout);
            }
            return {};
        }

        Result<void> stop() {
            Command cmd{};
            cmd.type = CommandType::stop;
            if (!queue_.push(cmd)) {
                return unexpected(Errc::timeout);
            }
            return {};
        }

        Result<void> pause() {
            Command cmd{};
            cmd.type = CommandType::pause;
            if (!queue_.push(cmd)) {
                return unexpected(Errc::timeout);
            }
            return {};
        }

        Result<void> resume() {
            Command cmd{};
            cmd.type = CommandType::resume;
            if (!queue_.push(cmd)) {
                return unexpected(Errc::timeout);
            }
            return {};
        }

        Result<void> set_eq(const EqConfig& eq) {
            Command cmd{};
            cmd.type = CommandType::set_eq;
            cmd.eq = eq;
            if (!queue_.push(cmd)) {
                return unexpected(Errc::timeout);
            }
            return {};
        }

        Result<void> set_volume(std::uint8_t percent) {
            Command cmd{};
            cmd.type = CommandType::set_volume;
            cmd.volume = percent;
            if (!queue_.push(cmd)) {
                return unexpected(Errc::timeout);
            }
            return {};
        }

        Result<void> seek_ms(std::uint64_t ms) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening) {
                return unexpected(Errc::bad_state);
            }
            if (!is_wav_ && !is_flac_ && !is_mp3_) {
                return unexpected(Errc::not_supported);
            }
            if (total_frames_ == 0) {
                return unexpected(Errc::not_supported);
            }
            Command cmd{};
            cmd.type = CommandType::seek_ms;
            cmd.seek_ms = ms;
            if (!queue_.push(cmd)) {
                return unexpected(Errc::timeout);
            }
            return {};
        }

        Result<void> reconfigure_format(const AudioFormat& input_fmt) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening) {
                return unexpected(Errc::bad_state);
            }
            Command cmd{};
            cmd.type = CommandType::reconfigure;
            cmd.fmt = input_fmt;
            if (!queue_.push(cmd)) {
                return unexpected(Errc::timeout);
            }
            return {};
        }

        Result<void> reconfigure_output(std::uint32_t fixed_rate, std::uint32_t fade_in_ms) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening) {
                return unexpected(Errc::bad_state);
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

            const std::size_t water = data_plane_.fifo_capacity()
                ? data_plane_.fifo().size_bytes()
                : 0;
            stats_.min_water = std::min(stats_.min_water, water);
            stats_.max_water = std::max(stats_.max_water, water);

            if (sink_.consume_underrun_flag()) {
                stats_.underrun_count = sink_.underrun_count();
            }

            if (state_ == PlayerState::buffering) {
                while (data_plane_.fifo_capacity() &&
                       data_plane_.fifo().size_bytes() < data_plane_.high_water()) {
                    refill_once();
                    if (!running_) break;
                    if (!has_more_data_ && data_plane_.fifo().size_bytes() == 0) break;
                }
                if (data_plane_.fifo_capacity() &&
                    data_plane_.fifo().size_bytes() >= data_plane_.high_water()) {
                    if (!sink_.start()) {
                        set_error(Errc::io_error, PlayerErrorStage::sink_start);
                        return;
                    }
                    state_ = PlayerState::playing;
                }
                return;
            }

            if (state_ == PlayerState::playing) {
                if (water <= data_plane_.low_water() ||
                    stats_.underrun_count != last_underrun_seen_) {
                    last_underrun_seen_ = stats_.underrun_count;
#if CHARM_AUDIO_ENABLE_STRESS
                    if (stress_ms_ > 0) {
                        stress_delay_ms(static_cast<std::uint32_t>(stress_dist_(rng_)));
                    }
#endif
                    refill_once();
                }
            }

            if (!has_more_data_ && data_plane_.fifo_capacity() &&
                data_plane_.fifo().size_bytes() == 0) {
                stop_internal();
            }
        }

        PlayerState state() const noexcept { return state_; }

        bool is_running() const noexcept {
            return state_ != PlayerState::idle && state_ != PlayerState::error;
        }

        std::uint64_t total_frames() const noexcept { return total_frames_; }

        AudioFormat input_format() const noexcept { return input_fmt_; }

        Errc last_error() const noexcept { return last_err_; }
        PlayerErrorStage last_error_stage() const noexcept { return last_err_stage_; }

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
            snap.water_bytes = data_plane_.fifo_capacity()
                ? data_plane_.fifo().size_bytes()
                : 0;
            snap.low_water = data_plane_.low_water();
            snap.high_water = data_plane_.high_water();
            snap.fifo_capacity = data_plane_.fifo_capacity();
            snap.period_frames = data_plane_.period_frames();
            snap.chunk_frames = data_plane_.chunk_frames();

            if (reset_window) {
                stats_.min_water = data_plane_.fifo_capacity();
                stats_.max_water = 0;
            }
            return snap;
        }

    private:
#if CHARM_AUDIO_ENABLE_STRESS
        void stress_delay_ms(std::uint32_t ms) noexcept {
            if (ms == 0) return;
            const auto start = clock_.now_ms();
            const auto probe = clock_.now_ms();
            if (start == 0 && probe == 0) return;
            while (true) {
                const auto now = clock_.now_ms();
                if (now < start) break;
                if (now - start >= ms) break;
            }
        }

        void seed_rng() noexcept {
            const auto seed = static_cast<std::uint32_t>(clock_.now_us());
            rng_.seed(seed);
        }
#endif
        enum class CommandType : std::uint8_t { play, stop, pause, resume, seek_ms, reconfigure, set_eq, set_volume };

        struct Command {
            CommandType type{};
            FixedString<kMaxPath> path{};
            std::uint64_t seek_ms{0};
            AudioFormat fmt{};
            EqConfig eq{};
            std::uint8_t volume{100};
        };

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

        void init_spectrum_window() noexcept {
            constexpr float kPi = 3.14159265358979323846f;
            for (std::size_t i = 0; i < spectrum_fft_size; ++i) {
                const float phase = static_cast<float>(i) / static_cast<float>(spectrum_fft_size - 1);
                spectrum_window_[i] = 0.5f - 0.5f * std::cos(phase * 2.0f * kPi);
            }
        }

        static media::StreamFormat to_stream_format(const AudioFormat& fmt) noexcept {
            media::StreamFormat out{};
            out.kind = media::StreamKind::audio;
            out.rate = fmt.rate;
            out.channels = fmt.channels;
            out.bits_per_sample = static_cast<std::uint16_t>(fmt.bytes_per_sample() * 8u);
            return out;
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
                    eq_dirty_ = true;
                } else if (cmd->type == CommandType::set_volume) {
                    volume_percent_ = std::min<std::uint8_t>(cmd->volume, 100);
                    volume_gain_ = static_cast<float>(volume_percent_) / 100.0f;
                }
            }
            if (eq_dirty_ && output_fmt_.rate != 0) {
                update_eq_filters();
            }
        }

        void handle_play(const char* path) {
            stop_internal();
            state_ = PlayerState::opening;
            last_err_ = Errc::ok;
            last_err_stage_ = PlayerErrorStage::none;

            if (!src_.open(path)) {
                set_error(Errc::io_error, PlayerErrorStage::open_source);
                return;
            }
            src_iface_ = media::make_stream_source_ref(src_);
            wav_filter_.close();
            flac_filter_.close();
            mp3_filter_.close();

            is_flac_ = ends_with_icase(path, ".flac");
            is_wav_ = ends_with_icase(path, ".wav");
            is_mp3_ = ends_with_icase(path, ".mp3");

            if (!is_flac_ && !is_wav_ && !is_mp3_) {
                std::array<std::byte, 12> header{};
                auto pos = src_iface_.tell();
                auto read = src_iface_.read(std::span<std::byte>(header.data(), header.size()));
                if (read && *read >= 4) {
                    const auto b0 = static_cast<unsigned char>(header[0]);
                    const auto b1 = static_cast<unsigned char>(header[1]);
                    const auto b2 = static_cast<unsigned char>(header[2]);
                    const auto b3 = static_cast<unsigned char>(header[3]);
                    if (b0 == 'f' && b1 == 'L' && b2 == 'a' && b3 == 'C') {
                        is_flac_ = true;
                    } else if (b0 == 'I' && b1 == 'D' && b2 == '3') {
                        is_mp3_ = true;
                    } else if (b0 == 0xFF && (b1 & 0xE0) == 0xE0) {
                        is_mp3_ = true;
                    } else if (read && *read >= 12) {
                        const auto b8 = static_cast<unsigned char>(header[8]);
                        const auto b9 = static_cast<unsigned char>(header[9]);
                        const auto b10 = static_cast<unsigned char>(header[10]);
                        const auto b11 = static_cast<unsigned char>(header[11]);
                        if (b0 == 'R' && b1 == 'I' && b2 == 'F' && b3 == 'F' &&
                            b8 == 'W' && b9 == 'A' && b10 == 'V' && b11 == 'E') {
                            is_wav_ = true;
                        }
                    }
                }
                if (pos) {
                    (void)src_iface_.seek(*pos, media::SeekWhence::set);
                } else {
                    (void)src_iface_.seek(0, media::SeekWhence::set);
                }
            }

            if (!is_flac_ && !is_wav_ && !is_mp3_) {
                set_error(Errc::not_supported, PlayerErrorStage::unsupported_format);
                return;
            }

            if (is_flac_) {
                const auto info = flac_filter_.open(src_iface_);
                if (!info) {
                    set_error(Errc::decode_error, PlayerErrorStage::decode_open);
                    return;
                }
                const auto fmt = flac_filter_.format();
                input_fmt_.rate = fmt.rate;
                input_fmt_.channels = fmt.channels;
                input_fmt_.sample_type = SampleType::s16;
                has_more_data_ = true;
                total_frames_ = flac_filter_.total_frames();
            } else if (is_mp3_) {
                const auto info = mp3_filter_.open(src_iface_);
                if (!info) {
                    set_error(Errc::decode_error, PlayerErrorStage::decode_open);
                    return;
                }
                const auto fmt = mp3_filter_.format();
                input_fmt_.rate = fmt.rate;
                input_fmt_.channels = fmt.channels;
                input_fmt_.sample_type = SampleType::s16;
                has_more_data_ = true;
                total_frames_ = mp3_filter_.total_frames();
            } else {
                const auto info = wav_filter_.open(src_iface_);
                if (!info) {
                    set_error(Errc::decode_error, PlayerErrorStage::wav_parse);
                    return;
                }
                const auto fmt = wav_filter_.format();
                if (fmt.bits_per_sample != 16) {
                    set_error(Errc::not_supported, PlayerErrorStage::wav_bits);
                    return;
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
            update_eq_filters();

            if (!configure_buffers()) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_config);
                return;
            }

            SinkConfig cfg{};
            cfg.format = to_stream_format(output_fmt_);
            cfg.period_frames = config_.preferred_period_frames;
            if (!sink_.open(cfg)) {
                set_error(Errc::io_error, PlayerErrorStage::sink_open);
                return;
            }
            std::uint32_t period_frames = sink_.actual_period_frames();
            if (period_frames == 0) {
                period_frames = output_fmt_.rate / 100;
            }
            const std::uint32_t chunk_frames = period_frames * config_.profile.chunk_mult;
            const std::size_t chunk_bytes =
                static_cast<std::size_t>(chunk_frames) * output_fmt_.frame_size();
            if (!allocate_buffers(chunk_bytes, chunk_frames)) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_alloc);
                return;
            }
            data_plane_.update_period(period_frames, chunk_frames, chunk_bytes, output_fmt_);
            sink_.set_fill_callback(
                data_plane_.pump().fill_callback(),
                &data_plane_.pump());

            stats_ = {};
            stats_.min_water = data_plane_.fifo_capacity();
            stats_.max_water = 0;
            last_underrun_seen_ = 0;
            fade_in_remaining_frames_ = fade_in_total_frames();

            if (is_wav_) {
                auto seek = src_iface_.seek(static_cast<std::int64_t>(data_offset_), media::SeekWhence::set);
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
            if (data_plane_.fifo_capacity()) data_plane_.clear_fifo();
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
                auto res = src_iface_.seek(static_cast<std::int64_t>(data_offset_ + clamped), media::SeekWhence::set);
                if (!res) {
                    set_error(Errc::io_error, PlayerErrorStage::seek);
                    return;
                }
            } else if (is_flac_) {
                auto res = flac_filter_.seek_pcm_frame(clamped_frames);
                if (!res) {
                    set_error(Errc::decode_error, PlayerErrorStage::seek);
                    running_ = false;
                    return;
                }
                has_more_data_ = true;
            } else if (is_mp3_) {
                auto res = mp3_filter_.seek_pcm_frame(clamped_frames);
                if (!res) {
                    set_error(Errc::io_error, PlayerErrorStage::seek);
                    running_ = false;
                    return;
                }
                has_more_data_ = true;
            } else {
                return;
            }
            stats_.min_water = data_plane_.fifo_capacity();
            stats_.max_water = 0;
            if (resample_enabled_) {
                resampler_.reset();
                resample_cache_frames_ = 0;
            }
            last_decode_eos_ = false;
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
            if (data_plane_.fifo_capacity() &&
                data_plane_.fifo().size_bytes() >= data_plane_.high_water()) {
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
            if (data_plane_.fifo_capacity()) data_plane_.clear_fifo();

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
            update_eq_filters();

            if (config_.fail_reconfig_step == 1) {
                set_error(Errc::bad_state, PlayerErrorStage::reconfigure);
                return;
            }

            if (!configure_buffers()) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_config);
                return;
            }

            SinkConfig cfg{};
            cfg.format = to_stream_format(output_fmt_);
            cfg.period_frames = config_.preferred_period_frames;
            if (!sink_.open(cfg)) {
                set_error(Errc::io_error, PlayerErrorStage::sink_open);
                return;
            }
            std::uint32_t period_frames = sink_.actual_period_frames();
            if (period_frames == 0) {
                period_frames = output_fmt_.rate / 100;
            }
            const std::uint32_t chunk_frames = period_frames * config_.profile.chunk_mult;
            const std::size_t chunk_bytes =
                static_cast<std::size_t>(chunk_frames) * output_fmt_.frame_size();
            if (!allocate_buffers(chunk_bytes, chunk_frames)) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_alloc);
                return;
            }
            data_plane_.update_period(period_frames, chunk_frames, chunk_bytes, output_fmt_);
            sink_.set_fill_callback(
                data_plane_.pump().fill_callback(),
                &data_plane_.pump());

            stats_.min_water = data_plane_.fifo_capacity();
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
            flac_filter_.close();
            mp3_filter_.close();
            wav_filter_.close();
            src_.close();
            src_iface_ = {};
            if (data_plane_.fifo_capacity()) data_plane_.clear_fifo();
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
            last_decode_eos_ = false;
            last_err_ = Errc::ok;
            last_err_stage_ = PlayerErrorStage::none;
            for (auto& biquad : eq_biquads_) {
                biquad.enabled = false;
                biquad.reset();
            }
            eq_ready_ = false;
            eq_dirty_ = false;
            spectrum_pos_ = 0;
            spectrum_ready_.store(false, std::memory_order_relaxed);
            state_ = PlayerState::idle;
        }

        void set_error(Errc code, PlayerErrorStage stage) noexcept {
            last_err_ = code;
            last_err_stage_ = stage;
            state_ = PlayerState::error;
        }

        bool configure_buffers() {
            if (output_fmt_.rate > kMaxRate || input_fmt_.rate > kMaxRate) return false;
            if (output_fmt_.channels == 0 || output_fmt_.channels > kMaxChannels) return false;
            if (input_fmt_.channels == 0 || input_fmt_.channels > kMaxChannels) return false;
            if (config_.profile.chunk_mult == 0 || config_.profile.chunk_mult > kMaxChunkMult) return false;
            if (config_.profile.fifo_ms == 0 || config_.profile.fifo_ms > kMaxFifoMs) return false;
            const std::size_t fifo_capacity = ms_to_bytes(config_.profile.fifo_ms, output_fmt_);
            if (fifo_capacity > kMaxFifoBytes) return false;
            if (!fifo_storage_.resize(fifo_capacity)) return false;
            const std::size_t low_water = ms_to_bytes(config_.profile.low_ms, output_fmt_);
            const std::size_t high_water = ms_to_bytes(config_.profile.high_ms, output_fmt_);
            std::uint32_t period_frames = config_.preferred_period_frames != 0
                ? config_.preferred_period_frames
                : (output_fmt_.rate / 100);
            if (period_frames > kMaxPeriodFrames) return false;
            const std::uint32_t chunk_frames = period_frames * config_.profile.chunk_mult;
            const std::size_t chunk_bytes =
                static_cast<std::size_t>(chunk_frames) * output_fmt_.frame_size();
            if (!data_plane_.configure(
                    std::span<std::byte>(fifo_storage_.data(), fifo_capacity),
                    fifo_capacity,
                    low_water,
                    high_water,
                    period_frames,
                    chunk_frames,
                    chunk_bytes,
                    output_fmt_)) {
                return false;
            }
            return allocate_buffers(chunk_bytes, chunk_frames);
        }

        bool allocate_buffers(std::size_t chunk_bytes, std::uint32_t chunk_frames) {
            const std::size_t out_samples = chunk_bytes / sizeof(std::int16_t);
            if (!s16_out_.resize(out_samples)) return false;
            if (!s32_out_.resize(out_samples)) return false;
            if (resample_enabled_) {
                input_chunk_frames_ = static_cast<std::size_t>(
                    (static_cast<std::uint64_t>(chunk_frames) * input_fmt_.rate) / output_fmt_.rate) + 2;
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
                input_chunk_frames_ = chunk_frames;
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
            if (!data_plane_.fifo_capacity()) return;
            if (!has_more_data_) return;
            auto& fifo = data_plane_.fifo();
            std::size_t writable = std::min(fifo.free_bytes(), data_plane_.chunk_bytes());
            writable = (writable / output_fmt_.frame_size()) * output_fmt_.frame_size();
            if (writable == 0) {
                stats_.overrun_count++;
                return;
            }

            const auto t0 = clock_.now_us();
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
                if (last_decode_eos_ && (!resample_enabled_ || resample_cache_frames_ == 0)) {
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
                auto view = fifo.writable_view();
                bytes_written = std::min(bytes_written, view.a.size() + view.b.size());
                if (bytes_written > 0) {
                    push_spectrum_samples(std::span<const std::byte>(
                        reinterpret_cast<const std::byte*>(s16_out_.data()),
                        bytes_written));
                }

                std::size_t written = 0;
                const std::size_t a = std::min(view.a.size(), bytes_written);
                std::memcpy(view.a.data(), reinterpret_cast<std::byte*>(s16_out_.data()), a);
                written += a;

                if (written < bytes_written && !view.b.empty()) {
                    const std::size_t b = std::min(view.b.size(), bytes_written - written);
                    std::memcpy(view.b.data(), reinterpret_cast<std::byte*>(s16_out_.data()) + written, b);
                    written += b;
                }
                fifo.commit_write(written);
            }

            const auto t1 = clock_.now_us();
            const double dt_ms = static_cast<double>(t1 - t0) / 1000.0;
            update_refill_stats(dt_ms);
        }

        std::size_t read_flac(std::size_t frames) {
            if (frames == 0) return 0;
            const std::size_t frame_bytes = input_fmt_.channels * sizeof(std::int32_t);
            const std::size_t out_frames = std::min(frames, s32_in_.size() / input_fmt_.channels);
            const std::size_t out_bytes = out_frames * frame_bytes;
            auto res = flac_filter_.process(std::span<const std::byte>{},
                std::span<std::byte>(reinterpret_cast<std::byte*>(s32_in_.data()), out_bytes));
            if (!res) {
                state_ = PlayerState::error;
                running_ = false;
                return 0;
            }
            last_decode_eos_ = res->end_of_stream;
            const std::size_t produced = res->produced - (res->produced % frame_bytes);
            return produced / frame_bytes;
        }

        std::size_t read_wav(std::size_t frames) {
            if (frames == 0) return 0;
            const std::size_t bytes_per_frame = input_fmt_.frame_size();
            const std::size_t max_bytes = std::min(frames * bytes_per_frame, raw_.size());
            const auto res = wav_filter_.process(std::span<const std::byte>{},
                std::span<std::byte>(raw_.data(), max_bytes));
            if (!res) {
                state_ = PlayerState::error;
                running_ = false;
                return 0;
            }
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
            if (!res) {
                state_ = PlayerState::error;
                running_ = false;
                return 0;
            }
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

        std::size_t quantize_s32(std::size_t frames) {
            apply_eq_s32(frames);
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
                if (volume_gain_ != 1.0f) {
                    v = static_cast<std::int32_t>(static_cast<float>(v) * volume_gain_);
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
        media::StreamSourceRef src_iface_{};
        FlacFilter flac_filter_{};
        Mp3Filter mp3_filter_{};
        WavFilter wav_filter_{};
        SinkType sink_{};
        AudioDataPlane data_plane_{};

        AudioFormat input_fmt_{};
        AudioFormat output_fmt_{};
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
        Errc last_err_{Errc::ok};
        PlayerErrorStage last_err_stage_{PlayerErrorStage::none};
        EqConfig eq_{};
        std::array<Biquad, EqConfig::max_bands> eq_biquads_{};
        bool eq_ready_{false};
        bool eq_dirty_{false};
        std::uint8_t volume_percent_{100};
        float volume_gain_{1.0f};

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
        bool last_decode_eos_{false};

        std::uint32_t stress_ms_{0};
        charm::system::ClockRef clock_{};
#if CHARM_AUDIO_ENABLE_STRESS
        std::mt19937 rng_{};
        std::uniform_int_distribution<int> stress_dist_{0, 0};
#endif
    };
}

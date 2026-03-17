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

#ifndef CHARM_AUDIO_LOG
#define CHARM_AUDIO_LOG 0
#endif

#if CHARM_AUDIO_ENABLE_STRESS
#include <random>
#endif

export module audio.player;

import audio.decode_pipe;
import audio.data_plane;
import audio.eq;
import audio.format;
import audio.pump;
import audio.result;
import alg_fft;
import charm.system.clock;
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

namespace {
    constexpr bool kAudioLogEnabled = CHARM_AUDIO_LOG != 0;

    void dump_path_escaped(const char* path) {
        if (!kAudioLogEnabled) {
            return;
        }
        if (!path) {
            std::printf("(null)");
            return;
        }
        for (const unsigned char ch : std::string_view{path}) {
            if (std::isprint(ch)) {
                std::printf("%c", static_cast<char>(ch));
            } else {
                std::printf("\\x%02X", static_cast<unsigned int>(ch));
            }
        }
    }
}

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
        std::uint32_t graph_block_frames{0};
        OutputMode output_mode{OutputMode::follow_input};
        std::uint32_t fixed_rate{0};
        std::uint32_t fade_in_ms{0};
        std::uint16_t force_channels{0};
        bool capture_output{true};
        std::uint8_t fail_reconfig_step{0};
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
        PumpStats pump{};
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
            if (!config_.capture_output) {
                data_plane_.set_capture_output(false);
            }
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

        Result<void> set_dc_block(bool enabled) {
            Command cmd{};
            cmd.type = CommandType::set_dc_block;
            cmd.flag = enabled;
            if (!queue_.push(cmd)) {
                return unexpected(Errc::timeout);
            }
            return {};
        }

        Result<void> set_soft_clip(bool enabled, float threshold) {
            Command cmd{};
            cmd.type = CommandType::set_soft_clip;
            cmd.flag = enabled;
            cmd.value = threshold;
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

        void shutdown() noexcept { stop_internal(); }

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

            const bool underrun_flag = sink_.consume_underrun_flag();
            if (underrun_flag) {
                stats_.underrun_count = sink_.underrun_count();
            }

            if (state_ == PlayerState::buffering) {
                buffer_until_high();
                return;
            }

            if (state_ == PlayerState::playing) {
                const bool low_water = water <= data_plane_.low_water();
                const bool underrun_seen = underrun_flag ||
                    (stats_.underrun_count != last_underrun_seen_);
                if (underrun_seen) {
                    last_underrun_seen_ = stats_.underrun_count;
                }
                if (low_water || underrun_seen) {
#if CHARM_AUDIO_ENABLE_STRESS
                    if (stress_ms_ > 0) {
                        stress_delay_ms(static_cast<std::uint32_t>(stress_dist_(rng_)));
                    }
#endif
                    (void)sink_.stop();
                    state_ = PlayerState::buffering;
                    buffer_until_high();
                    return;
                }
            }

            if (!data_plane_.has_more_data() && data_plane_.fifo_capacity() &&
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
            if (!config_.capture_output) {
                spectrum_enabled_.store(false, std::memory_order_relaxed);
                data_plane_.set_capture_output(false);
                spectrum_ready_.store(false, std::memory_order_relaxed);
                return;
            }
            spectrum_enabled_.store(on, std::memory_order_relaxed);
            data_plane_.set_capture_output(on);
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
            snap.pump = data_plane_.pump().snapshot();
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
        enum class CommandType : std::uint8_t {
            play,
            stop,
            pause,
            resume,
            seek_ms,
            reconfigure,
            set_eq,
            set_volume,
            set_dc_block,
            set_soft_clip
        };

        struct Command {
            CommandType type{};
            FixedString<kMaxPath> path{};
            std::uint64_t seek_ms{0};
            AudioFormat fmt{};
            EqConfig eq{};
            std::uint8_t volume{100};
            bool flag{false};
            float value{0.0f};
        };

        std::uint32_t compute_chunk_frames(std::uint32_t period_frames) const noexcept {
            std::uint32_t chunk_frames = period_frames * config_.profile.chunk_mult;
            if (config_.output_mode == OutputMode::fixed_rate &&
                output_fmt_.rate > 0 &&
                input_fmt_.rate > output_fmt_.rate) {
                const std::uint32_t max_input_frames = static_cast<std::uint32_t>(kMaxChunkFrames + 2);
                const std::uint64_t max_out_frames = (static_cast<std::uint64_t>(max_input_frames - 2)
                    * output_fmt_.rate) / input_fmt_.rate;
                const std::uint32_t capped = static_cast<std::uint32_t>(
                    std::max<std::uint64_t>(1, max_out_frames));
                if (chunk_frames > capped) {
                    chunk_frames = capped;
                }
            }
            return chunk_frames;
        }

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

        static bool is_power_of_two(std::uint32_t value) noexcept {
            return value != 0 && (value & (value - 1u)) == 0;
        }

        std::uint32_t select_graph_block_frames(std::uint32_t period_frames) const noexcept {
            if (period_frames == 0) return 0;
            std::uint32_t block = config_.graph_block_frames;
            if (block == 0) {
                block = std::min<std::uint32_t>(period_frames, 128);
            } else if (block > period_frames) {
                block = period_frames;
            } else if (block < period_frames && !is_power_of_two(block)) {
                block = std::min<std::uint32_t>(period_frames, 128);
            }
            return block;
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

        void apply_dsp_settings() noexcept {
            data_plane_.set_eq(eq_);
            data_plane_.maybe_update_eq();
            data_plane_.set_volume_gain(volume_gain_);
            data_plane_.enable_dc_block(dc_block_enabled_);
            data_plane_.enable_soft_clip(soft_clip_enabled_);
            data_plane_.set_soft_clip_threshold(soft_clip_threshold_);
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
                    data_plane_.set_eq(eq_);
                } else if (cmd->type == CommandType::set_volume) {
                    volume_percent_ = std::min<std::uint8_t>(cmd->volume, 100);
                    volume_gain_ = static_cast<float>(volume_percent_) / 100.0f;
                    data_plane_.set_volume_gain(volume_gain_);
                } else if (cmd->type == CommandType::set_dc_block) {
                    dc_block_enabled_ = cmd->flag;
                    data_plane_.enable_dc_block(dc_block_enabled_);
                } else if (cmd->type == CommandType::set_soft_clip) {
                    soft_clip_enabled_ = cmd->flag;
                    soft_clip_threshold_ = std::clamp(cmd->value, 0.0f, 1.0f);
                    data_plane_.enable_soft_clip(soft_clip_enabled_);
                    data_plane_.set_soft_clip_threshold(soft_clip_threshold_);
                }
            }
            data_plane_.maybe_update_eq();
        }

        void handle_play(const char* path) {
            (void)last_path_.assign("");
            if (path) {
                (void)last_path_.assign(path);
            }
            stop_internal();
            state_ = PlayerState::opening;
            last_err_ = Errc::ok;
            last_err_stage_ = PlayerErrorStage::none;

            if (!src_.open(path)) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] open failed: ");
                    dump_path_escaped(path);
                    std::printf("\n");
                }
#endif
                set_error(Errc::io_error, PlayerErrorStage::open_source);
                return;
            }
            src_iface_ = media::make_stream_source_ref(src_);
            data_plane_.close_source();

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
            SourceKind kind = SourceKind::wav;
            if (is_flac_) {
                kind = SourceKind::flac;
            } else if (is_mp3_) {
                kind = SourceKind::mp3;
            }

            const auto opened = data_plane_.open_source(src_iface_, kind);
            if (!opened) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] decode open failed (%d): ", static_cast<int>(opened.error()));
                    dump_path_escaped(path);
                    std::printf("\n");
                }
#endif
                if (kind == SourceKind::wav && opened.error() == Errc::not_supported) {
                    set_error(Errc::not_supported, PlayerErrorStage::wav_bits);
                } else if (kind == SourceKind::wav) {
                    set_error(opened.error(), PlayerErrorStage::wav_parse);
                } else {
                    set_error(opened.error(), PlayerErrorStage::decode_open);
                }
                return;
            }

            input_fmt_ = data_plane_.input_format();
            output_fmt_ = data_plane_.output_format();
            total_frames_ = data_plane_.total_frames();

            if (!data_plane_.configure_output(
                    config_.force_channels,
                    config_.output_mode == OutputMode::fixed_rate,
                    config_.fixed_rate)) {
                set_error(Errc::bad_state, PlayerErrorStage::channel_convert);
                return;
            }
            input_fmt_ = data_plane_.input_format();
            output_fmt_ = data_plane_.output_format();
            total_frames_ = data_plane_.total_frames();
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
            data_plane_.set_graph_block_frames(select_graph_block_frames(period_frames));
            const std::uint32_t chunk_frames = compute_chunk_frames(period_frames);
            const std::size_t chunk_bytes =
                static_cast<std::size_t>(chunk_frames) * output_fmt_.frame_size();
            if (!data_plane_.update_period(period_frames, chunk_frames, chunk_bytes, output_fmt_)) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_alloc);
                return;
            }
            apply_dsp_settings();
            sink_.set_fill_callback(
                data_plane_.pump().fill_callback(),
                &data_plane_.pump());

            stats_ = {};
            stats_.min_water = data_plane_.fifo_capacity();
            stats_.max_water = 0;
            last_underrun_seen_ = 0;
            data_plane_.reset_fade(fade_in_total_frames());

            running_ = true;
            state_ = PlayerState::buffering;
        }

        void handle_seek(std::uint64_t ms) {
            if (state_ == PlayerState::idle) return;
            (void)sink_.stop();
            if (data_plane_.fifo_capacity()) data_plane_.clear_fifo();
            const std::uint64_t frames = (static_cast<std::uint64_t>(input_fmt_.rate) * ms) / 1000;
            const std::uint64_t total = data_plane_.total_frames();
            std::uint64_t clamped_frames = (total > 0) ? std::min(frames, total) : frames;
            if (total > 0 && clamped_frames >= total) {
                clamped_frames = total - 1;
            }
            auto res = data_plane_.seek_frames(clamped_frames);
            if (!res) {
                set_error(res.error(), PlayerErrorStage::seek);
                running_ = false;
                return;
            }
            stats_.min_water = data_plane_.fifo_capacity();
            stats_.max_water = 0;
            data_plane_.reset_fade(fade_in_total_frames());
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
            if (!data_plane_.configure_output(
                    config_.force_channels,
                    config_.output_mode == OutputMode::fixed_rate,
                    config_.fixed_rate)) {
                set_error(Errc::bad_state, PlayerErrorStage::channel_convert);
                return;
            }
            input_fmt_ = data_plane_.input_format();
            output_fmt_ = data_plane_.output_format();
            total_frames_ = data_plane_.total_frames();
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
            data_plane_.set_graph_block_frames(select_graph_block_frames(period_frames));
            const std::uint32_t chunk_frames = compute_chunk_frames(period_frames);
            const std::size_t chunk_bytes =
                static_cast<std::size_t>(chunk_frames) * output_fmt_.frame_size();
            if (!data_plane_.update_period(period_frames, chunk_frames, chunk_bytes, output_fmt_)) {
                set_error(Errc::bad_state, PlayerErrorStage::buffer_alloc);
                return;
            }
            apply_dsp_settings();
            sink_.set_fill_callback(
                data_plane_.pump().fill_callback(),
                &data_plane_.pump());

            stats_.min_water = data_plane_.fifo_capacity();
            stats_.max_water = 0;
            last_underrun_seen_ = sink_.underrun_count();
            data_plane_.reset_fade(fade_in_total_frames());
            state_ = PlayerState::buffering;
        }

        void stop_internal() {
            if (state_ == PlayerState::idle) return;
            state_ = PlayerState::stopping;
            (void)sink_.stop();
            sink_.close();
            data_plane_.close_source();
            src_.close();
            src_iface_ = {};
            if (data_plane_.fifo_capacity()) data_plane_.clear_fifo();
            running_ = false;
            is_flac_ = false;
            is_wav_ = false;
            is_mp3_ = false;
            total_frames_ = 0;
            last_err_ = Errc::ok;
            last_err_stage_ = PlayerErrorStage::none;
            spectrum_pos_ = 0;
            spectrum_ready_.store(false, std::memory_order_relaxed);
            state_ = PlayerState::idle;
        }

        void set_error(Errc code, PlayerErrorStage stage) noexcept {
            last_err_ = code;
            last_err_stage_ = stage;
            state_ = PlayerState::error;
#if defined(_WIN32)
            if (kAudioLogEnabled) {
                std::printf("[audio] error stage=%u err=%u path=",
                    static_cast<unsigned int>(stage),
                    static_cast<unsigned int>(code));
                dump_path_escaped(last_path_.c_str());
                std::printf("\n");
            }
#endif
        }

        void buffer_until_high() {
            constexpr std::size_t kMaxRefillLoops = 8;
            std::size_t loops = 0;
            std::size_t last_size = data_plane_.fifo().size_bytes();
            while (data_plane_.fifo_capacity() &&
                   data_plane_.fifo().size_bytes() < data_plane_.high_water()) {
                refill_once();
                if (!running_) break;
                const std::size_t size = data_plane_.fifo().size_bytes();
                if (!data_plane_.has_more_data()) break;
                if (size == last_size) {
                    if (++loops >= kMaxRefillLoops) break;
                } else {
                    loops = 0;
                    last_size = size;
                }
            }
            if (data_plane_.fifo_capacity() &&
                !data_plane_.has_more_data() &&
                data_plane_.fifo().size_bytes() == 0) {
                stop_internal();
                return;
            }
            if (data_plane_.fifo_capacity() &&
                (data_plane_.fifo().size_bytes() >= data_plane_.high_water() ||
                 (!data_plane_.has_more_data() && data_plane_.fifo().size_bytes() > 0))) {
                if (!sink_.start()) {
                    set_error(Errc::io_error, PlayerErrorStage::sink_start);
                    return;
                }
                state_ = PlayerState::playing;
            }
        }

        bool configure_buffers() {
            if (output_fmt_.rate > kMaxRate ||
                (config_.output_mode == OutputMode::follow_input && input_fmt_.rate > kMaxRate)) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] buffer_config rate in=%u out=%u max=%u\n",
                        static_cast<unsigned int>(input_fmt_.rate),
                        static_cast<unsigned int>(output_fmt_.rate),
                        static_cast<unsigned int>(kMaxRate));
                }
#endif
                return false;
            }
            if (output_fmt_.channels == 0 || output_fmt_.channels > kMaxChannels) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] buffer_config out channels=%u max=%u\n",
                        static_cast<unsigned int>(output_fmt_.channels),
                        static_cast<unsigned int>(kMaxChannels));
                }
#endif
                return false;
            }
            if (input_fmt_.channels == 0 || input_fmt_.channels > kMaxChannels) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] buffer_config in channels=%u max=%u\n",
                        static_cast<unsigned int>(input_fmt_.channels),
                        static_cast<unsigned int>(kMaxChannels));
                }
#endif
                return false;
            }
            if (config_.profile.chunk_mult == 0 || config_.profile.chunk_mult > kMaxChunkMult) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] buffer_config chunk_mult=%u max=%u\n",
                        static_cast<unsigned int>(config_.profile.chunk_mult),
                        static_cast<unsigned int>(kMaxChunkMult));
                }
#endif
                return false;
            }
            if (config_.profile.fifo_ms == 0 || config_.profile.fifo_ms > kMaxFifoMs) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] buffer_config fifo_ms=%u max=%u\n",
                        static_cast<unsigned int>(config_.profile.fifo_ms),
                        static_cast<unsigned int>(kMaxFifoMs));
                }
#endif
                return false;
            }
            const std::size_t fifo_capacity = ms_to_bytes(config_.profile.fifo_ms, output_fmt_);
            if (fifo_capacity > kMaxFifoBytes) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] buffer_config fifo_bytes=%zu max=%zu\n",
                        fifo_capacity, static_cast<std::size_t>(kMaxFifoBytes));
                }
#endif
                return false;
            }
            if (!fifo_storage_.resize(fifo_capacity)) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] buffer_config fifo_storage resize failed\n");
                }
#endif
                return false;
            }
            const std::size_t low_water = std::min(
                ms_to_bytes(config_.profile.low_ms, output_fmt_), fifo_capacity);
            std::size_t high_water = std::min(
                ms_to_bytes(config_.profile.high_ms, output_fmt_), fifo_capacity);
            if (high_water < low_water) high_water = low_water;
            std::uint32_t period_frames = config_.preferred_period_frames != 0
                ? config_.preferred_period_frames
                : (output_fmt_.rate / 100);
            if (period_frames > kMaxPeriodFrames) {
#if defined(_WIN32)
                if (kAudioLogEnabled) {
                    std::printf("[audio] buffer_config period_frames=%u max=%u\n",
                        static_cast<unsigned int>(period_frames),
                        static_cast<unsigned int>(kMaxPeriodFrames));
                }
#endif
                return false;
            }
            const std::uint32_t requested_chunk_frames = period_frames * config_.profile.chunk_mult;
            std::uint32_t chunk_frames = compute_chunk_frames(period_frames);
            (void)requested_chunk_frames;
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
            return true;
        }

        void refill_once() {
            if (!data_plane_.fifo_capacity()) return;
            if (!data_plane_.has_more_data()) return;
            auto& fifo = data_plane_.fifo();
            std::size_t writable = std::min(fifo.producer_free_bytes(), data_plane_.chunk_bytes());
            writable = (writable / output_fmt_.frame_size()) * output_fmt_.frame_size();
            if (writable == 0) {
                stats_.overrun_count++;
                return;
            }

            const auto t0 = clock_.now_us();
            const std::size_t bytes_written = data_plane_.refill();
            if (bytes_written > 0) {
                const auto out = data_plane_.last_output();
                if (!out.empty()) {
                    push_spectrum_samples(out);
                }
            }

            const auto t1 = clock_.now_us();
            const double dt_ms = static_cast<double>(t1 - t0) / 1000.0;
            update_refill_stats(dt_ms);
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
        FixedString<kMaxPath> last_path_{};
        media::StreamSourceRef src_iface_{};
        SinkType sink_{};
        AudioDataPlane data_plane_{};

        AudioFormat input_fmt_{};
        AudioFormat output_fmt_{};

        StaticBuffer<std::byte, kMaxFifoBytes> fifo_storage_{};

        std::uint64_t total_frames_{0};
        Errc last_err_{Errc::ok};
        PlayerErrorStage last_err_stage_{PlayerErrorStage::none};
        EqConfig eq_{};
        std::uint8_t volume_percent_{100};
        float volume_gain_{1.0f};
        bool dc_block_enabled_{true};
        bool soft_clip_enabled_{true};
        float soft_clip_threshold_{0.85f};

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
        std::uint64_t last_underrun_seen_{0};

        std::uint32_t stress_ms_{0};
        charm::system::ClockRef clock_{};
#if CHARM_AUDIO_ENABLE_STRESS
        std::mt19937 rng_{};
        std::uniform_int_distribution<int> stress_dist_{0, 0};
#endif
    };
}

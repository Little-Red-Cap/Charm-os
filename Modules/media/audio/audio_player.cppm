module;

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <expected>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <thread>
#include <vector>

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
import audio.source.file;

export namespace audio {
    enum class OutputMode : std::uint8_t {
        follow_input,
        fixed_rate
    };

    enum class PauseMode : std::uint8_t {
        hard,
        soft
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
            : config_(config) {}

        ~AudioPlayer() { stop_internal(); }

        Result<void> play(const char* path) {
            if (!path || !*path) return std::unexpected(Err{Errc::invalid_arg, 0});
            Command cmd{};
            cmd.type = CommandType::play;
            cmd.path = path;
            queue_.push_back(cmd);
            return {};
        }

        Result<void> stop() {
            Command cmd{};
            cmd.type = CommandType::stop;
            queue_.push_back(cmd);
            return {};
        }

        Result<void> pause(PauseMode mode = PauseMode::hard) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening || state_ == PlayerState::error) {
                return std::unexpected(Err{Errc::bad_state, 0});
            }
            if (state_ == PlayerState::paused) {
                pause_mode_ = mode;
                paused_soft_ = (mode == PauseMode::soft);
                return {};
            }
            Command cmd{};
            cmd.type = CommandType::pause;
            cmd.pause_mode = mode;
            queue_.push_back(cmd);
            return {};
        }

        Result<void> resume() {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening || state_ == PlayerState::error) {
                if (eos_reached_) {
                    return std::unexpected(Err{Errc::end_of_stream, 0});
                }
                return std::unexpected(Err{Errc::bad_state, 0});
            }
            if (state_ != PlayerState::paused) {
                return {};
            }
            Command cmd{};
            cmd.type = CommandType::resume;
            queue_.push_back(cmd);
            return {};
        }

        Result<void> seek_ms(std::uint64_t ms) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening) {
                return std::unexpected(Err{Errc::bad_state, 0});
            }
            if (!is_wav_) {
                return std::unexpected(Err{Errc::not_supported, 0});
            }
            Command cmd{};
            cmd.type = CommandType::seek_ms;
            cmd.seek_ms = ms;
            queue_.push_back(cmd);
            return {};
        }

        Result<void> reconfigure_format(const AudioFormat& input_fmt) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening) {
                return std::unexpected(Err{Errc::bad_state, 0});
            }
            Command cmd{};
            cmd.type = CommandType::reconfigure;
            cmd.fmt = input_fmt;
            queue_.push_back(cmd);
            return {};
        }

        Result<void> reconfigure_output(std::uint32_t fixed_rate, std::uint32_t fade_in_ms) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening) {
                return std::unexpected(Err{Errc::bad_state, 0});
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
            stress_dist_ = std::uniform_int_distribution<int>(0, static_cast<int>(stress_ms_));
        }

        void tick() {
            process_commands();

            if (state_ == PlayerState::idle || state_ == PlayerState::error) {
                return;
            }

            const std::size_t water = fifo_ ? fifo_->size_bytes() : 0;
            stats_.min_water = std::min(stats_.min_water, water);
            stats_.max_water = std::max(stats_.max_water, water);

            if (sink_.consume_underrun_flag()) {
                stats_.underrun_count = sink_.underrun_count();
            }

            if (state_ == PlayerState::buffering) {
                while (fifo_ && fifo_->size_bytes() < high_water_) {
                    refill_once();
                    if (!running_) break;
                    if (!has_more_data_ && fifo_->size_bytes() == 0) break;
                }
                if (fifo_ && fifo_->size_bytes() >= high_water_) {
                    if (!sink_.start()) {
                        state_ = PlayerState::error;
                        return;
                    }
                    state_ = PlayerState::playing;
                }
                return;
            }

            if (state_ == PlayerState::paused) {
                return;
            }

            if (state_ == PlayerState::playing) {
                if (water <= low_water_ || stats_.underrun_count != last_underrun_seen_) {
                    last_underrun_seen_ = stats_.underrun_count;
                    if (stress_ms_ > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(stress_dist_(rng_)));
                    }
                    refill_once();
                }
            }

            if (!has_more_data_ && fifo_ && fifo_->size_bytes() == 0) {
                stop_internal(StopReason::eof);
            }
        }

        PlayerState state() const noexcept { return state_; }

        bool is_running() const noexcept {
            return state_ != PlayerState::idle && state_ != PlayerState::error;
        }

        PlayerSnapshot snapshot(bool reset_window) {
            PlayerSnapshot snap{};
            snap.stats = stats_;
            snap.callback = sink_.callback_stats();
            snap.input_fmt = input_fmt_;
            snap.output_fmt = output_fmt_;
            snap.water_bytes = fifo_ ? fifo_->size_bytes() : 0;
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
        enum class CommandType : std::uint8_t { play, stop, pause, resume, seek_ms, reconfigure };
        enum class StopReason : std::uint8_t { user, eof };

        struct Command {
            CommandType type{};
            std::string path{};
            std::uint64_t seek_ms{0};
            AudioFormat fmt{};
            PauseMode pause_mode{PauseMode::hard};
        };

        static std::size_t fill_from_fifo(std::span<std::byte> dst, void* user) noexcept {
            auto* self = static_cast<AudioPlayer*>(user);
            if (!self || !self->fifo_) return 0;
            const std::size_t frame = self->output_fmt_.frame_size();
            if (frame == 0) return 0;

            std::size_t need = dst.size() - (dst.size() % frame);
            std::size_t filled = 0;

            while (filled < need) {
                auto v = self->fifo_->readable_view();
                if (v.a.empty() && v.b.empty()) break;

                auto copy_one = [&](std::span<std::byte> src) {
                    std::size_t n = std::min(src.size(), need - filled);
                    n -= n % frame;
                    if (n == 0) return;
                    std::memcpy(dst.data() + filled, src.data(), n);
                    self->fifo_->commit_read(n);
                    filled += n;
                };

                if (!v.a.empty()) copy_one(v.a);
                else if (!v.b.empty()) copy_one(v.b);
            }

            return filled;
        }

        void process_commands() {
            while (!queue_.empty()) {
                Command cmd = std::move(queue_.front());
                queue_.pop_front();
                if (cmd.type == CommandType::play) {
                    handle_play(cmd.path.c_str());
                } else if (cmd.type == CommandType::stop) {
                    stop_internal(StopReason::user);
                } else if (cmd.type == CommandType::pause) {
                    handle_pause(cmd.pause_mode);
                } else if (cmd.type == CommandType::resume) {
                    handle_resume();
                } else if (cmd.type == CommandType::seek_ms) {
                    handle_seek(cmd.seek_ms);
                } else if (cmd.type == CommandType::reconfigure) {
                    handle_reconfigure(cmd.fmt);
                }
            }
        }

        void handle_play(const char* path) {
            stop_internal(StopReason::user);
            state_ = PlayerState::opening;
            eos_reached_ = false;

            if (!src_.open(path)) {
                state_ = PlayerState::error;
                return;
            }

            is_flac_ = ends_with_icase(path, ".flac");
            is_wav_ = ends_with_icase(path, ".wav");
            is_mp3_ = ends_with_icase(path, ".mp3");

            if (!is_flac_ && !is_wav_ && !is_mp3_) {
                state_ = PlayerState::error;
                return;
            }

            if (is_flac_) {
                const auto info = flac_.open(src_);
                if (!info) {
                    state_ = PlayerState::error;
                    return;
                }
                input_fmt_.rate = info->sample_rate;
                input_fmt_.channels = info->channels;
                input_fmt_.sample_type = SampleType::s16;
                has_more_data_ = true;
            } else if (is_mp3_) {
                const auto info = mp3_.open(src_);
                if (!info) {
                    state_ = PlayerState::error;
                    return;
                }
                input_fmt_.rate = info->sample_rate;
                input_fmt_.channels = info->channels;
                input_fmt_.sample_type = SampleType::s16;
                has_more_data_ = true;
            } else {
                const auto info = parse_wav(src_);
                if (!info) {
                    state_ = PlayerState::error;
                    return;
                }
                if (info->bits_per_sample != 16) {
                    state_ = PlayerState::error;
                    return;
                }
                input_fmt_.rate = info->sample_rate;
                input_fmt_.channels = info->channels;
                input_fmt_.sample_type = SampleType::s16;
                data_offset_ = info->data_offset;
                data_size_ = info->data_size;
                remaining_bytes_ = info->data_size;
                has_more_data_ = remaining_bytes_ > 0;
            }

            output_fmt_ = input_fmt_;
            if (config_.force_channels != 0) {
                output_fmt_.channels = config_.force_channels;
            }

            if (!configure_channel_convert()) {
                state_ = PlayerState::error;
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

            configure_buffers();

            SinkConfig cfg{};
            cfg.fmt = output_fmt_;
            cfg.preferred_period_frames = config_.preferred_period_frames;
            if (!sink_.open(cfg)) {
                state_ = PlayerState::error;
                return;
            }
            period_frames_ = sink_.actual_period_frames();
            if (period_frames_ == 0) {
                period_frames_ = output_fmt_.rate / 100;
            }
            chunk_frames_ = period_frames_ * config_.profile.chunk_mult;
            chunk_bytes_ = static_cast<std::size_t>(chunk_frames_) * output_fmt_.frame_size();
            allocate_buffers(chunk_bytes_);

            sink_.set_fill_callback(&AudioPlayer::fill_from_fifo, this);

            stats_ = {};
            stats_.min_water = fifo_capacity_;
            stats_.max_water = 0;
            last_underrun_seen_ = 0;
            fade_in_remaining_frames_ = fade_in_total_frames();

            if (is_wav_) {
                auto seek = src_.seek(static_cast<std::int64_t>(data_offset_), SEEK_SET);
                if (!seek) {
                    state_ = PlayerState::error;
                    return;
                }
            }

            running_ = true;
            state_ = PlayerState::buffering;
        }

        void handle_pause(PauseMode mode) {
            if (state_ == PlayerState::idle || state_ == PlayerState::opening || state_ == PlayerState::error) {
                return;
            }
            pause_mode_ = mode;
            paused_soft_ = (mode == PauseMode::soft);
            if (mode == PauseMode::hard) {
                (void)sink_.stop();
            }
            state_ = PlayerState::paused;
        }

        void handle_resume() {
            if (state_ != PlayerState::paused) {
                return;
            }
            if (!has_more_data_ && fifo_ && fifo_->size_bytes() == 0) {
                stop_internal(StopReason::eof);
                return;
            }
            paused_soft_ = false;
            state_ = PlayerState::buffering;
        }

        void handle_seek(std::uint64_t ms) {
            if (!is_wav_ || state_ == PlayerState::idle) return;
            (void)sink_.stop();
            if (fifo_) fifo_->clear();
            const std::uint64_t frames = (static_cast<std::uint64_t>(input_fmt_.rate) * ms) / 1000;
            const std::uint64_t offset = frames * input_fmt_.frame_size();
            const std::uint64_t clamped = std::min<std::uint64_t>(offset, data_size_);
            remaining_bytes_ = static_cast<std::size_t>(data_size_ - clamped);
            has_more_data_ = remaining_bytes_ > 0;
            auto res = src_.seek(static_cast<std::int64_t>(data_offset_ + clamped), SEEK_SET);
            if (!res) {
                state_ = PlayerState::error;
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

        void handle_reconfigure(const AudioFormat& input_fmt) {
            if (input_fmt.channels == 0 || input_fmt.rate == 0) {
                state_ = PlayerState::error;
                return;
            }
            (void)sink_.stop();
            sink_.clear_underrun_flag();
            sink_.close();
            if (fifo_) fifo_->clear();

            input_fmt_ = input_fmt;
            output_fmt_ = input_fmt_;
            if (config_.force_channels != 0) {
                output_fmt_.channels = config_.force_channels;
            }
            if (!configure_channel_convert()) {
                state_ = PlayerState::error;
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
                state_ = PlayerState::error;
                return;
            }

            configure_buffers();

            SinkConfig cfg{};
            cfg.fmt = output_fmt_;
            cfg.preferred_period_frames = config_.preferred_period_frames;
            if (!sink_.open(cfg)) {
                state_ = PlayerState::error;
                return;
            }
            period_frames_ = sink_.actual_period_frames();
            if (period_frames_ == 0) {
                period_frames_ = output_fmt_.rate / 100;
            }
            chunk_frames_ = period_frames_ * config_.profile.chunk_mult;
            chunk_bytes_ = static_cast<std::size_t>(chunk_frames_) * output_fmt_.frame_size();
            allocate_buffers(chunk_bytes_);
            sink_.set_fill_callback(&AudioPlayer::fill_from_fifo, this);

            stats_.min_water = fifo_capacity_;
            stats_.max_water = 0;
            last_underrun_seen_ = sink_.underrun_count();
            fade_in_remaining_frames_ = fade_in_total_frames();
            state_ = PlayerState::buffering;
        }

        void stop_internal(StopReason reason) {
            if (state_ == PlayerState::idle) return;
            state_ = PlayerState::stopping;
            (void)sink_.stop();
            sink_.close();
            flac_.close();
            mp3_.close();
            src_.close();
            if (fifo_) fifo_->clear();
            running_ = false;
            has_more_data_ = false;
            is_flac_ = false;
            is_wav_ = false;
            is_mp3_ = false;
            data_offset_ = 0;
            data_size_ = 0;
            remaining_bytes_ = 0;
            resample_enabled_ = false;
            resample_cache_frames_ = 0;
            eos_reached_ = (reason == StopReason::eof);
            state_ = PlayerState::idle;
        }

        void configure_buffers() {
            fifo_capacity_ = ms_to_bytes(config_.profile.fifo_ms, output_fmt_);
            fifo_ = std::make_unique<PcmFifo>(fifo_capacity_);
            low_water_ = ms_to_bytes(config_.profile.low_ms, output_fmt_);
            high_water_ = ms_to_bytes(config_.profile.high_ms, output_fmt_);
            period_frames_ = config_.preferred_period_frames != 0
                ? config_.preferred_period_frames
                : (output_fmt_.rate / 100);
            chunk_frames_ = period_frames_ * config_.profile.chunk_mult;
            chunk_bytes_ = static_cast<std::size_t>(chunk_frames_) * output_fmt_.frame_size();
            allocate_buffers(chunk_bytes_);
        }

        void allocate_buffers(std::size_t chunk_bytes) {
            const std::size_t out_samples = chunk_bytes / sizeof(std::int16_t);
            s16_out_.resize(out_samples);
            s32_out_.resize(out_samples);
            if (resample_enabled_) {
                input_chunk_frames_ = static_cast<std::size_t>(
                    (static_cast<std::uint64_t>(chunk_frames_) * input_fmt_.rate) / output_fmt_.rate) + 2;
                if (input_chunk_frames_ < 2) input_chunk_frames_ = 2;
                const std::size_t in_samples = input_chunk_frames_ * input_fmt_.channels;
                const std::size_t conv_samples = input_chunk_frames_ * output_fmt_.channels;
                raw_.resize(in_samples * sizeof(std::int16_t));
                s16_in_.resize(in_samples);
                s32_in_.resize(in_samples);
                s32_conv_.resize(conv_samples);
                s32_work_.resize(conv_samples + output_fmt_.channels);
                resample_cache_.resize(conv_samples + output_fmt_.channels);
            } else {
                input_chunk_frames_ = chunk_frames_;
                const std::size_t in_samples = input_chunk_frames_ * input_fmt_.channels;
                const std::size_t conv_samples = input_chunk_frames_ * output_fmt_.channels;
                raw_.resize(in_samples * sizeof(std::int16_t));
                s16_in_.resize(in_samples);
                s32_in_.resize(in_samples);
                s32_conv_.resize(conv_samples);
            }
        }

        void refill_once() {
            if (!fifo_) return;
            if (!has_more_data_) return;
            std::size_t writable = std::min(fifo_->free_bytes(), chunk_bytes_);
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
                auto view = fifo_->writable_view();
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
                fifo_->commit_write(written);
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
            if (s16_in_.size() < samples) {
                s16_in_.resize(samples);
                s32_in_.resize(samples);
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
            if (s32_conv_.size() < out_samples) {
                s32_conv_.resize(out_samples);
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

            s32_work_.resize((total_frames + 1) * channels);
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
                resample_cache_.resize(remaining * channels);
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
            const std::string value{text};
            const std::string suf{suffix};
            if (value.size() < suf.size()) return false;
            const std::size_t start = value.size() - suf.size();
            for (std::size_t i = 0; i < suf.size(); ++i) {
                const char a = static_cast<char>(std::tolower(value[start + i]));
                const char b = static_cast<char>(std::tolower(suf[i]));
                if (a != b) return false;
            }
            return true;
        }

        PlayerConfig config_{};
        PlayerStats stats_{};
        PlayerState state_{PlayerState::idle};
        std::deque<Command> queue_{};

        FileDataSource src_{};
        FlacDecoder flac_{};
        Mp3Decoder mp3_{};
        Sdl3AudioSink sink_{};
        std::unique_ptr<PcmFifo> fifo_{};

        AudioFormat input_fmt_{};
        AudioFormat output_fmt_{};
        std::size_t fifo_capacity_{0};
        std::size_t low_water_{0};
        std::size_t high_water_{0};
        std::uint32_t period_frames_{0};
        std::uint32_t chunk_frames_{0};
        std::size_t chunk_bytes_{0};
        std::size_t input_chunk_frames_{0};

        std::vector<std::byte> raw_{};
        std::vector<std::int16_t> s16_in_{};
        std::vector<std::int16_t> s16_out_{};
        std::vector<std::int32_t> s32_in_{};
        std::vector<std::int32_t> s32_out_{};
        std::vector<std::int32_t> s32_conv_{};
        std::vector<std::int32_t> s32_work_{};
        std::vector<std::int32_t> resample_cache_{};
        std::size_t resample_cache_frames_{0};
        LinearResamplerS32 resampler_{};
        bool resample_enabled_{false};
        std::uint64_t fade_in_remaining_frames_{0};
        ChannelConverterS32 channel_conv_{};

        std::size_t data_offset_{0};
        std::size_t data_size_{0};
        std::size_t remaining_bytes_{0};

        bool is_flac_{false};
        bool is_wav_{false};
        bool is_mp3_{false};
        bool running_{false};
        bool has_more_data_{false};
        std::uint64_t last_underrun_seen_{0};

        std::uint32_t stress_ms_{0};
        std::mt19937 rng_{static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())};
        std::uniform_int_distribution<int> stress_dist_{0, 0};

        PauseMode pause_mode_{PauseMode::hard};
        bool paused_soft_{false};
        bool eos_reached_{false};
    };
}

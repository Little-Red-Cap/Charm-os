module;
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

export module player.playback;

import audio.eq;
import audio.player;
import audio.result;
import charm.system.clock;
import player.fixed_string;
import player.product_policy;

namespace {
    void dump_path_escaped(const char* path) {
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

export namespace player {
    bool player_state_allows_seek(audio::PlayerState state) noexcept {
        return state == audio::PlayerState::playing
            || state == audio::PlayerState::buffering
            || state == audio::PlayerState::paused;
    }

    const char* audio_err_text(audio::Errc err) {
        switch (err) {
        case audio::Errc::ok: return "ok";
        case audio::Errc::perm: return "perm";
        case audio::Errc::noent: return "noent";
        case audio::Errc::io: return "io_error";
        case audio::Errc::again: return "again";
        case audio::Errc::nomem: return "nomem";
        case audio::Errc::busy: return "busy";
        case audio::Errc::exist: return "exist";
        case audio::Errc::notdir: return "notdir";
        case audio::Errc::isdir: return "isdir";
        case audio::Errc::inval: return "invalid_arg";
        case audio::Errc::rofs: return "rofs";
        case audio::Errc::nametoolong: return "nametoolong";
        case audio::Errc::nosys: return "nosys";
        case audio::Errc::notsup: return "not_supported";
        case audio::Errc::timeout: return "timeout";
        case audio::Errc::end_of_stream: return "end_of_stream";
        case audio::Errc::decode_error: return "decode_error";
        case audio::Errc::bad_state: return "bad_state";
        case audio::Errc::crc_error: return "crc_error";
        case audio::Errc::format_error: return "format_error";
        case audio::Errc::canceled: return "canceled";
        case audio::Errc::closed: return "closed";
        case audio::Errc::buffer_overflow: return "buffer_overflow";
        case audio::Errc::invalid_format: return "invalid_format";
        }
        return "unknown";
    }

    const char* audio_stage_text(audio::PlayerErrorStage stage) {
        switch (stage) {
        case audio::PlayerErrorStage::none: return "none";
        case audio::PlayerErrorStage::open_source: return "open_source";
        case audio::PlayerErrorStage::unsupported_format: return "unsupported_format";
        case audio::PlayerErrorStage::decode_open: return "decode_open";
        case audio::PlayerErrorStage::wav_parse: return "wav_parse";
        case audio::PlayerErrorStage::wav_bits: return "wav_bits";
        case audio::PlayerErrorStage::channel_convert: return "channel_convert";
        case audio::PlayerErrorStage::buffer_config: return "buffer_config";
        case audio::PlayerErrorStage::sink_open: return "sink_open";
        case audio::PlayerErrorStage::buffer_alloc: return "buffer_alloc";
        case audio::PlayerErrorStage::sink_start: return "sink_start";
        case audio::PlayerErrorStage::seek: return "seek";
        case audio::PlayerErrorStage::resume: return "resume";
        case audio::PlayerErrorStage::reconfigure: return "reconfigure";
        }
        return "unknown";
    }

    struct ProgressUpdate {
        bool updated{false};
        int value{0};
        int current_sec{0};
    };

    enum class PlaybackAction {
        start,
        pause,
        resume,
        stop,
        seek,
        toggle,
    };

    class PlaybackEngine {
    public:
        void bind_clock(charm::system::Clock& clock) noexcept { clock_.reset(clock); }
        void set_player(audio::AudioPlayer& p) noexcept { player_ = &p; }
        bool has_player() const noexcept { return player_ != nullptr; }

        void set_track_path(const char* path) noexcept { track_path_ = path; }
        void set_track_ready(bool ready) noexcept { track_ready_ = ready; }
        const char* track_path() const noexcept { return track_path_; }
        bool track_ready() const noexcept { return track_ready_; }

        bool playing() const noexcept { return playing_; }
        bool paused() const noexcept { return paused_; }
        bool duration_ready() const noexcept { return duration_ready_; }
        int duration_sec() const noexcept { return duration_sec_; }
        int current_sec() const noexcept { return current_sec_; }
        int volume_percent() const noexcept { return volume_percent_; }

        bool snapshot(audio::PlayerSnapshot& out) {
            if (!player_) return false;
            out = player_->snapshot(false);
            return true;
        }

        void reset_duration() noexcept {
            duration_ready_ = false;
            duration_sec_ = 0;
            current_sec_ = 0;
        }

        bool update_duration_from_player() {
            if (duration_ready_ || !player_) return false;
            const auto total = player_->total_frames();
            const auto fmt = player_->input_format();
            if (total == 0 || fmt.rate == 0) return false;
            const auto secs = static_cast<int>(total / fmt.rate);
            set_duration_ready_sec(secs);
            return true;
        }

        void set_duration_from_probe(int seconds) noexcept {
            set_duration_ready_sec(seconds);
            current_sec_ = 0;
        }

        ProgressUpdate update_progress() {
            ProgressUpdate out{};
            if (!playing_ || !player_) return out;
            (void)update_duration_from_player();
            if (!duration_ready_ || duration_sec_ <= 0) return out;
            const auto now_ms = clock_.now_ms();
            const std::uint64_t elapsed_ms = now_ms - start_ms_;
            const int elapsed = static_cast<int>(elapsed_ms / 1000);
            const int clamped = (elapsed > duration_sec_) ? duration_sec_ : elapsed;
            out.current_sec = clamped;
            out.value = (duration_sec_ > 0) ? static_cast<int>((clamped * 100) / duration_sec_) : 0;
            out.updated = true;
            current_sec_ = clamped;
            return out;
        }

        void set_current_sec(int sec) noexcept { current_sec_ = clamp_position_sec(sec); }

        bool is_seek_ready() const {
            if (!player_) return false;
            return player_state_allows_seek(player_->state());
        }

        bool request_seek(int target_sec, FixedString<128>& out_status) {
            if (!player_) {
                out_status.assign("No player");
                return false;
            }
            if (!is_seek_ready()) {
                out_status.assign("Seek not ready");
                return false;
            }
            if (target_sec < 0) {
                out_status.assign("Seek target invalid");
                return false;
            }
            (void)update_duration_from_player();
            if (!duration_ready_ || duration_sec_ <= 0) {
                out_status.assign("Seek duration unknown");
                return false;
            }
            const int clamped = clamp_position_sec(target_sec);
            const auto res = player_->seek_ms(static_cast<std::uint64_t>(clamped) * 1000);
            if (!res) {
                out_status.assign("Seek unsupported");
                return false;
            }
            current_sec_ = clamped;
            if (playing_) {
                start_ms_ = clock_.now_ms()
                    - static_cast<std::uint64_t>(current_sec_) * 1000;
            }
            return true;
        }

        bool set_volume(int percent, FixedString<128>& out_status) {
            if (!player_) {
                out_status.assign("No player");
                return false;
            }
            const int clamped = std::clamp(percent, 0, 100);
            const auto res = player_->set_volume(static_cast<std::uint8_t>(clamped));
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Volume failed (%s)", audio_err_text(res.error()));
                out_status.assign(buf);
                return false;
            }
            volume_percent_ = clamped;
            return true;
        }

        bool set_eq(const audio::EqConfig& eq, FixedString<128>& out_status) {
            if (!player_) {
                out_status.assign("No player");
                return false;
            }
            const auto res = player_->set_eq(eq);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "EQ failed (%s)", audio_err_text(res.error()));
                out_status.assign(buf);
                return false;
            }
            return true;
        }

        bool set_dc_block(bool enabled, FixedString<128>& out_status) {
            if (!player_) {
                out_status.assign("No player");
                return false;
            }
            const auto res = player_->set_dc_block(enabled);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "DC-block failed (%s)", audio_err_text(res.error()));
                out_status.assign(buf);
                return false;
            }
            return true;
        }

        bool set_soft_clip(bool enabled, int threshold_percent, FixedString<128>& out_status) {
            if (!player_) {
                out_status.assign("No player");
                return false;
            }
            const int clamped = std::clamp(threshold_percent, 0, 100);
            const float threshold = static_cast<float>(clamped) / 100.0f;
            const auto res = player_->set_soft_clip(enabled, threshold);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Soft clip failed (%s)", audio_err_text(res.error()));
                out_status.assign(buf);
                return false;
            }
            return true;
        }

        bool apply_action(PlaybackAction action, int seek_sec, FixedString<128>& out_status) {
            switch (action) {
            case PlaybackAction::toggle:
                if (playing_) {
                    return pause_playback(out_status);
                }
                if (paused_) {
                    return resume_playback(out_status);
                }
                return start_playback(out_status);
            case PlaybackAction::start:
                return start_playback(out_status);
            case PlaybackAction::pause:
                return pause_playback(out_status);
            case PlaybackAction::resume:
                return resume_playback(out_status);
            case PlaybackAction::stop:
                stop_playback();
                out_status.assign("Stopped");
                return true;
            case PlaybackAction::seek:
                if (!request_seek(seek_sec, out_status)) {
                    return false;
                }
                out_status.assign(paused_ ? "Paused" : "Playing");
                return true;
            }
            return false;
        }

        bool start_playback(FixedString<128>& out_status) {
            if (!player_) {
                out_status.assign("No player");
                return false;
            }
            if (!track_path_) {
                out_status.assign("No track");
                return false;
            }
            if (!track_ready_) {
                out_status.assign("Track not ready");
                if constexpr (product_policy::playback_log) {
                    std::printf("[player] track not ready: ");
                    dump_path_escaped(track_path_);
                    std::printf("\n");
                }
                return false;
            }
            (void)player_->stop();
            if constexpr (product_policy::playback_log) {
                std::printf("[player] play: ");
                dump_path_escaped(track_path_);
                std::printf("\n");
            }
            auto res = player_->play(track_path_);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Play failed (%s)", audio_err_text(res.error()));
                out_status.assign(buf);
                if constexpr (product_policy::playback_log) {
                    std::printf("[player] play failed (%s): ", audio_err_text(res.error()));
                    dump_path_escaped(track_path_);
                    std::printf("\n");
                }
                return false;
            }
            playing_ = true;
            paused_ = false;
            start_ms_ = clock_.now_ms();
            current_sec_ = 0;
            out_status.assign("Opening");
            return true;
        }

        bool pause_playback(FixedString<128>& out_status) {
            if (!player_ || !playing_) return false;
            (void)update_progress();
            auto res = player_->pause();
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Pause failed (%s)", audio_err_text(res.error()));
                out_status.assign(buf);
                return false;
            }
            playing_ = false;
            paused_ = true;
            out_status.assign("Paused");
            return true;
        }

        bool resume_playback(FixedString<128>& out_status) {
            if (!player_ || !paused_) return false;
            auto res = player_->resume();
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Resume failed (%s)", audio_err_text(res.error()));
                out_status.assign(buf);
                return false;
            }
            paused_ = false;
            playing_ = true;
            start_ms_ = clock_.now_ms()
                - static_cast<std::uint64_t>(current_sec_) * 1000;
            out_status.assign("Playing");
            return true;
        }

        void stop_playback() {
            if (player_) {
                (void)player_->stop();
            }
            playing_ = false;
            paused_ = false;
            current_sec_ = 0;
        }

    private:
        charm::system::ClockRef clock_{};
        audio::AudioPlayer* player_{nullptr};
        const char* track_path_{nullptr};
        bool track_ready_{false};
        bool playing_{false};
        bool paused_{false};
        bool duration_ready_{false};
        int duration_sec_{180};
        int current_sec_{0};
        int volume_percent_{80};
        std::uint64_t start_ms_{0};

        void set_duration_ready_sec(int seconds) noexcept {
            duration_sec_ = (seconds > 0) ? seconds : 1;
            duration_ready_ = true;
            current_sec_ = clamp_position_sec(current_sec_);
        }

        int clamp_position_sec(int sec) const noexcept {
            if (sec < 0) {
                return 0;
            }
            if (duration_ready_ && duration_sec_ > 0 && sec > duration_sec_) {
                return duration_sec_;
            }
            return sec;
        }
    };
}

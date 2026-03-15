module;
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

export module player.playback;

import audio.eq;
import audio.player;
import audio.result;

export namespace player {
    const char* audio_err_text(audio::Errc err) {
        switch (err) {
        case audio::Errc::ok: return "ok";
        case audio::Errc::invalid_arg: return "invalid_arg";
        case audio::Errc::not_supported: return "not_supported";
        case audio::Errc::io_error: return "io_error";
        case audio::Errc::decode_error: return "decode_error";
        case audio::Errc::bad_state: return "bad_state";
        case audio::Errc::timeout: return "timeout";
        case audio::Errc::end_of_stream: return "end_of_stream";
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
        void set_player(audio::AudioPlayer& p) noexcept { player_ = &p; }
        bool has_player() const noexcept { return player_ != nullptr; }

        void set_track_path(const char* path) noexcept { track_path_ = path; }
        void set_track_ready(bool ready) noexcept { track_ready_ = ready; }
        const char* track_path() const noexcept { return track_path_; }
        bool track_ready() const noexcept { return track_ready_; }

        bool playing() const noexcept { return playing_; }
        bool paused() const noexcept { return paused_; }
        int duration_sec() const noexcept { return duration_sec_; }
        int current_sec() const noexcept { return current_sec_; }
        int volume_percent() const noexcept { return volume_percent_; }

        void reset_duration() noexcept {
            duration_ready_ = false;
            duration_sec_ = 180;
        }

        bool update_duration_from_player() {
            if (duration_ready_ || !player_) return false;
            const auto total = player_->total_frames();
            const auto fmt = player_->input_format();
            if (total == 0 || fmt.rate == 0) return false;
            const auto secs = static_cast<int>(total / fmt.rate);
            duration_sec_ = (secs > 0) ? secs : 1;
            duration_ready_ = true;
            return true;
        }

        ProgressUpdate update_progress() {
            ProgressUpdate out{};
            if (!playing_ || !player_) return out;
            const auto now = std::chrono::steady_clock::now();
            const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - start_).count());
            const int clamped = (elapsed > duration_sec_) ? duration_sec_ : elapsed;
            out.current_sec = clamped;
            out.value = (duration_sec_ > 0) ? static_cast<int>((clamped * 100) / duration_sec_) : 0;
            out.updated = true;
            current_sec_ = clamped;
            return out;
        }

        void set_current_sec(int sec) noexcept { current_sec_ = sec; }

        bool is_seek_ready() const {
            if (!player_) return false;
            const auto st = player_->state();
            return st == audio::PlayerState::playing || st == audio::PlayerState::buffering;
        }

        bool request_seek(int target_sec, std::string& out_status) {
            if (!player_ || target_sec < 0) return false;
            const auto res = player_->seek_ms(static_cast<std::uint64_t>(target_sec) * 1000);
            if (!res) {
                out_status = "Seek unsupported";
                return false;
            }
            return true;
        }

        bool set_volume(int percent, std::string& out_status) {
            if (!player_) {
                out_status = "No player";
                return false;
            }
            const int clamped = std::clamp(percent, 0, 100);
            const auto res = player_->set_volume(static_cast<std::uint8_t>(clamped));
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Volume failed (%s)", audio_err_text(res.error()));
                out_status = buf;
                return false;
            }
            volume_percent_ = clamped;
            return true;
        }

        bool set_eq(const audio::EqConfig& eq, std::string& out_status) {
            if (!player_) {
                out_status = "No player";
                return false;
            }
            const auto res = player_->set_eq(eq);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "EQ failed (%s)", audio_err_text(res.error()));
                out_status = buf;
                return false;
            }
            return true;
        }

        bool apply_action(PlaybackAction action, int seek_sec, std::string& out_status) {
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
                out_status = "Stopped";
                return true;
            case PlaybackAction::seek:
                if (!is_seek_ready()) {
                    out_status = "Seek not ready";
                    return false;
                }
                if (!request_seek(seek_sec, out_status)) {
                    return false;
                }
                current_sec_ = seek_sec;
                start_ = std::chrono::steady_clock::now() - std::chrono::seconds(current_sec_);
                out_status = "Playing";
                return true;
            }
            return false;
        }

        bool start_playback(std::string& out_status) {
            if (!player_) {
                out_status = "No player";
                return false;
            }
            if (!track_path_) {
                out_status = "No track";
                return false;
            }
            if (!track_ready_) {
                out_status = "Track not ready";
                return false;
            }
            (void)player_->stop();
            auto res = player_->play(track_path_);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Play failed (%s)", audio_err_text(res.error()));
                out_status = buf;
                return false;
            }
            playing_ = true;
            paused_ = false;
            start_ = std::chrono::steady_clock::now();
            current_sec_ = 0;
            out_status = "Opening";
            return true;
        }

        bool pause_playback(std::string& out_status) {
            if (!player_ || !playing_) return false;
            auto res = player_->pause();
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Pause failed (%s)", audio_err_text(res.error()));
                out_status = buf;
                return false;
            }
            playing_ = false;
            paused_ = true;
            out_status = "Paused";
            return true;
        }

        bool resume_playback(std::string& out_status) {
            if (!player_ || !paused_) return false;
            auto res = player_->resume();
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Resume failed (%s)", audio_err_text(res.error()));
                out_status = buf;
                return false;
            }
            paused_ = false;
            playing_ = true;
            start_ = std::chrono::steady_clock::now() - std::chrono::seconds(current_sec_);
            out_status = "Playing";
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
        audio::AudioPlayer* player_{nullptr};
        const char* track_path_{nullptr};
        bool track_ready_{false};
        bool playing_{false};
        bool paused_{false};
        bool duration_ready_{false};
        int duration_sec_{180};
        int current_sec_{0};
        int volume_percent_{80};
        std::chrono::steady_clock::time_point start_{};
    };
}

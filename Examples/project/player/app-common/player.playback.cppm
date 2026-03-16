module;
#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>

export module player.playback;

import audio.decode_pipe;
import audio.eq;
import audio.player;
import audio.result;
#if defined(CHARM_AUDIO_USE_VFS)
import audio.source.fs;
#else
import audio.source.file;
#endif
import media.stream.source;

export namespace player {
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

        bool snapshot(audio::PlayerSnapshot& out) {
            if (!player_) return false;
            out = player_->snapshot(false);
            return true;
        }

        void reset_duration() noexcept {
            duration_ready_ = false;
            duration_sec_ = 180;
        }

        bool probe_duration_from_path(const char* path) {
            if (!path || !*path) return false;
#if defined(CHARM_AUDIO_USE_VFS)
            audio::FsDataSource src{};
#else
            audio::FileDataSource src{};
#endif
            if (!src.open(path)) return false;
            auto ref = media::make_stream_source_ref(src);
            audio::SourceKind kind = audio::SourceKind::wav;
            if (ends_with_icase(path, ".flac")) {
                kind = audio::SourceKind::flac;
            } else if (ends_with_icase(path, ".mp3")) {
                kind = audio::SourceKind::mp3;
            } else if (ends_with_icase(path, ".wav")) {
                kind = audio::SourceKind::wav;
            } else {
                std::array<std::byte, 12> header{};
                auto pos = ref.tell();
                auto read = ref.read(std::span<std::byte>(header.data(), header.size()));
                if (read && *read >= 4) {
                    const auto b0 = static_cast<unsigned char>(header[0]);
                    const auto b1 = static_cast<unsigned char>(header[1]);
                    const auto b2 = static_cast<unsigned char>(header[2]);
                    const auto b3 = static_cast<unsigned char>(header[3]);
                    if (b0 == 'f' && b1 == 'L' && b2 == 'a' && b3 == 'C') {
                        kind = audio::SourceKind::flac;
                    } else if (b0 == 'I' && b1 == 'D' && b2 == '3') {
                        kind = audio::SourceKind::mp3;
                    } else if (b0 == 0xFF && (b1 & 0xE0) == 0xE0) {
                        kind = audio::SourceKind::mp3;
                    } else if (read && *read >= 12) {
                        const auto b8 = static_cast<unsigned char>(header[8]);
                        const auto b9 = static_cast<unsigned char>(header[9]);
                        const auto b10 = static_cast<unsigned char>(header[10]);
                        const auto b11 = static_cast<unsigned char>(header[11]);
                        if (b0 == 'R' && b1 == 'I' && b2 == 'F' && b3 == 'F' &&
                            b8 == 'W' && b9 == 'A' && b10 == 'V' && b11 == 'E') {
                            kind = audio::SourceKind::wav;
                        }
                    }
                }
                if (pos) {
                    (void)ref.seek(*pos, media::SeekWhence::set);
                } else {
                    (void)ref.seek(0, media::SeekWhence::set);
                }
            }

            audio::AudioDecodePipe probe{};
            auto opened = probe.open(ref, kind);
            if (!opened) return false;
            const auto total = probe.total_frames();
            const auto fmt = probe.input_format();
            if (total == 0 || fmt.rate == 0) return false;
            const auto secs = static_cast<int>(total / fmt.rate);
            duration_sec_ = (secs > 0) ? secs : 1;
            duration_ready_ = true;
            current_sec_ = 0;
            return true;
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

        bool set_dc_block(bool enabled, std::string& out_status) {
            if (!player_) {
                out_status = "No player";
                return false;
            }
            const auto res = player_->set_dc_block(enabled);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "DC-block failed (%s)", audio_err_text(res.error()));
                out_status = buf;
                return false;
            }
            return true;
        }

        bool set_soft_clip(bool enabled, int threshold_percent, std::string& out_status) {
            if (!player_) {
                out_status = "No player";
                return false;
            }
            const int clamped = std::clamp(threshold_percent, 0, 100);
            const float threshold = static_cast<float>(clamped) / 100.0f;
            const auto res = player_->set_soft_clip(enabled, threshold);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Soft clip failed (%s)", audio_err_text(res.error()));
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

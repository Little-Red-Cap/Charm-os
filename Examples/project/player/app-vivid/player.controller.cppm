module;
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <string_view>
#include <vector>

export module player.controller;

import audio.player;
import audio.result;
import charm.core.event;
import charm.core.handle;
import charm.core.soa_kernel;
import fs_core;
import fs_vfs;
import player.fs_utils;
import player.ui;

export namespace player {
    using namespace player::fs_utils;
    using namespace player::ui;

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

    struct UiHandles {
        WidgetHandle root{};
        WidgetHandle cover{};
        WidgetHandle title{};
        WidgetHandle subtitle{};
        WidgetHandle status{};
        WidgetHandle progress{};
        WidgetHandle time{};
        WidgetHandle list{};
        WidgetHandle list_title{};
        WidgetHandle list_hint{};
        WidgetHandle list_scroll{};
        WidgetHandle mode_hint{};
        WidgetHandle btn_prev{};
        WidgetHandle btn_pause{};
        WidgetHandle btn_next{};
        WidgetHandle btn_mode{};
        WidgetHandle controls{};
    };

    struct PlayerController {
        audio::AudioPlayer* player{nullptr};
        SoaKernel* kernel{nullptr};
        UiHandles handles{};
        bool playing{false};
        bool track_ready{false};
        std::chrono::steady_clock::time_point start{};
        int duration_sec{180};
        int current_sec{0};
        const char* track_path{nullptr};
        std::vector<std::string>* tracks{nullptr};
        std::vector<std::string> track_labels{};
        int track_index{0};
        std::string title_text{};
        std::string subtitle_text{};
        bool paused{false};
        bool fs_ready{false};
        int play_mode{0};
        bool duration_ready{false};
        bool ignore_list_select{false};
        int last_list_selected{-1};
        std::string mount_status{};
        std::mt19937 rng{static_cast<unsigned int>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count())};

        void bind_kernel(SoaKernel& k) {
            kernel = &k;
        }

        void set_label(WidgetHandle h, const char* text) {
            if (!kernel || !h) return;
            kernel->set_text(h, text);
        }

        void set_status(const char* text) { set_label(handles.status, text); }

        void set_play_button_text(bool playing_now) {
            set_label(handles.btn_pause, playing_now ? "Pause" : "Play");
        }

        void set_time_label(int elapsed_sec) {
            current_sec = elapsed_sec;
            char buf[32]{};
            const int total = duration_sec;
            const int cur_m = elapsed_sec / 60;
            const int cur_s = elapsed_sec % 60;
            const int total_m = total / 60;
            const int total_s = total % 60;
            std::snprintf(buf, sizeof(buf), "%d:%02d / %d:%02d", cur_m, cur_s, total_m, total_s);
            set_label(handles.time, buf);
        }

        void reset_duration() {
            duration_ready = false;
            duration_sec = 180;
        }

        static const char* play_mode_text(int mode) noexcept {
            switch (mode) {
            case 1: return "Single";
            case 2: return "Shuffle";
            default: return "Order";
            }
        }

        void update_play_mode_label() {
            char buf[32]{};
            std::snprintf(buf, sizeof(buf), "Mode: %s", play_mode_text(play_mode));
            set_label(handles.mode_hint, buf);
        }

        static const char* list_view_text(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<const PlayerController*>(ctx);
            if (!self) return "";
            if (index >= self->track_labels.size()) return "";
            return self->track_labels[index].c_str();
        }

        void rebuild_track_labels() {
            track_labels.clear();
            if (!tracks) return;
            track_labels.reserve(tracks->size());
            for (const auto& path : *tracks) {
                auto base = std::string_view{path};
                const auto pos = base.find_last_of("/\\");
                if (pos != std::string_view::npos) base = base.substr(pos + 1);
                track_labels.emplace_back(base);
            }
        }

        void sync_list_selection() {
            if (!kernel || !handles.list) return;
            if (track_index < 0 || track_index >= static_cast<int>(track_labels.size())) return;
            ignore_list_select = true;
            kernel->set_list_view_selected(handles.list, track_index);
            last_list_selected = track_index;
            ignore_list_select = false;
        }

        void refresh_list_view() {
            if (!kernel || !handles.list) return;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            kernel->set_list_view_source(handles.list,
                                         static_cast<std::uint16_t>(count),
                                         this,
                                         &PlayerController::list_view_text);
            kernel->set_list_row_height(handles.list, 34);
            kernel->set_scroll_step(handles.list, 34);
            if (count > 0) {
                sync_list_selection();
            } else {
                last_list_selected = -1;
            }
            update_list_title();
            update_list_placeholder();
        }

        void update_list_title() {
            if (!kernel) return;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            char buf[64]{};
            if (count > 0) {
                std::snprintf(buf, sizeof(buf), "Tracks (%d)", count);
                set_label(handles.list_title, buf);
            } else {
                set_label(handles.list_title, "Tracks");
            }
        }

        void update_list_placeholder() {
            if (!kernel || !handles.list_hint) return;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            const bool show = (count == 0);
            kernel->set_visible(handles.list_hint, show);
            if (!show) return;
            if (!mount_status.empty()) {
                set_label(handles.list_hint, mount_status.c_str());
                return;
            }
            set_label(handles.list_hint, fs_ready ? "No tracks in /music or /" : "FS not ready");
        }

        int resolve_next_track() {
            if (!tracks || tracks->empty()) return -1;
            const int count = static_cast<int>(tracks->size());
            if (play_mode == 1) {
                return track_index;
            }
            if (play_mode == 2) {
                if (count <= 1) return track_index;
                std::uniform_int_distribution<int> dist(0, count - 1);
                int next = track_index;
                for (int i = 0; i < 4 && next == track_index; ++i) {
                    next = dist(rng);
                }
                if (next == track_index) {
                    next = (track_index + 1) % count;
                }
                return next;
            }
            int next = track_index + 1;
            if (next >= count) next = 0;
            return next;
        }

        void handle_track_end() {
            if (!fs_ready || !tracks || tracks->empty()) {
                stop_playback();
                return;
            }
            const int next = resolve_next_track();
            if (next < 0) {
                stop_playback();
                return;
            }
            stop_playback();
            if (load_track_index(next)) {
                start_playback();
            }
        }

        void update_duration_from_player() {
            if (duration_ready || !player) return;
            const auto total = player->total_frames();
            const auto fmt = player->input_format();
            if (total == 0 || fmt.rate == 0) return;
            const auto secs = static_cast<int>(total / fmt.rate);
            duration_sec = (secs > 0) ? secs : 1;
            duration_ready = true;
            if (current_sec > duration_sec) {
                set_time_label(duration_sec);
                sync_progress_value(100);
            } else {
                set_time_label(current_sec);
            }
        }

        bool update_progress() {
            if (!playing || !kernel) return false;
            const auto now = std::chrono::steady_clock::now();
            const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - start).count());
            const int clamped = (elapsed > duration_sec) ? duration_sec : elapsed;
            const int value = (duration_sec > 0)
                ? static_cast<int>((clamped * 100) / duration_sec)
                : 0;
            kernel->set_value(handles.progress, value);
            set_time_label(clamped);
            return true;
        }

        void sync_progress_value(int value) {
            if (!kernel) return;
            kernel->set_value(handles.progress, value);
        }

        bool is_seek_ready() const {
            if (!player) return false;
            const auto st = player->state();
            return st == audio::PlayerState::playing || st == audio::PlayerState::buffering;
        }

        bool request_seek(int target_sec) {
            if (!player || target_sec < 0) return false;
            const auto res = player->seek_ms(static_cast<std::uint64_t>(target_sec) * 1000);
            if (!res) {
                set_status("Seek unsupported");
                return false;
            }
            set_status("Playing");
            return true;
        }

        void start_playback() {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            if (!player || !track_path) {
                set_status("No track");
                return;
            }
            if (!track_ready) {
                set_status("Track not ready");
                return;
            }
            (void)player->stop();
            auto res = player->play(track_path);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Play failed (%s)", audio_err_text(res.error()));
                set_status(buf);
                return;
            }
            playing = true;
            paused = false;
            start = std::chrono::steady_clock::now();
            set_status("Opening");
            set_play_button_text(true);
            set_time_label(0);
            sync_progress_value(0);
        }

        void pause_playback() {
            if (!player || !playing) return;
            auto res = player->pause();
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Pause failed (%s)", audio_err_text(res.error()));
                set_status(buf);
                return;
            }
            playing = false;
            paused = true;
            set_status("Paused");
            set_play_button_text(false);
        }

        void resume_playback() {
            if (!player || !paused) return;
            auto res = player->resume();
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Resume failed (%s)", audio_err_text(res.error()));
                set_status(buf);
                return;
            }
            paused = false;
            playing = true;
            start = std::chrono::steady_clock::now() - std::chrono::seconds(current_sec);
            set_status("Playing");
            set_play_button_text(true);
        }

        void stop_playback() {
            if (player) {
                (void)player->stop();
            }
            playing = false;
            paused = false;
            set_status("Stopped");
            set_play_button_text(false);
            set_time_label(0);
            sync_progress_value(0);
        }

        void set_track_labels(std::string_view vfs_path) {
            auto base = vfs_path;
            const auto pos = vfs_path.find_last_of("/\\");
            if (pos != std::string_view::npos) base = vfs_path.substr(pos + 1);
            title_text.assign(base.begin(), base.end());
            if (title_text.empty()) title_text = "Unknown Track";

            std::string_view ext{};
            const auto dot = base.find_last_of('.');
            if (dot != std::string_view::npos && dot + 1 < base.size()) {
                ext = base.substr(dot + 1);
            }
            subtitle_text.clear();
            if (!ext.empty()) {
                subtitle_text.assign(ext.begin(), ext.end());
                for (auto& ch : subtitle_text) {
                    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                }
            } else {
                subtitle_text = "UNKNOWN";
            }

            set_label(handles.title, title_text.c_str());
            set_label(handles.subtitle, subtitle_text.c_str());
        }

        bool load_track_index(int idx) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return false;
            }
            if (!tracks || tracks->empty()) return false;
            if (idx < 0) idx = 0;
            if (idx >= static_cast<int>(tracks->size())) idx = static_cast<int>(tracks->size()) - 1;
            track_index = idx;
            const auto& vfs_path = (*tracks)[track_index];
            track_path = vfs_path.c_str();
            set_track_labels(vfs_path);
            fs::File f{};
            auto st = fs::vfs_open(vfs_path, f);
            if (st) {
                (void)fs::vfs_close(f);
                track_ready = true;
            } else {
                track_ready = false;
            }
            if (track_ready) {
                set_status("Ready");
            } else {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Load failed (%s)", fs_err_text(st.err));
                set_status(buf);
            }
            set_play_button_text(false);
            set_time_label(0);
            sync_progress_value(0);
            playing = false;
            paused = false;
            reset_duration();
            sync_list_selection();
            return track_ready;
        }

        void switch_track(int delta) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            if (!tracks || tracks->empty()) return;
            const int count = static_cast<int>(tracks->size());
            int next = track_index + delta;
            if (next < 0) next = count - 1;
            if (next >= count) next = 0;
            const bool was_active = playing || paused;
            stop_playback();
            load_track_index(next);
            if (was_active && track_ready) {
                start_playback();
            }
        }

        void select_track_index(int idx) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            if (!tracks || tracks->empty()) return;
            const bool was_playing = playing;
            const bool was_paused = paused;
            stop_playback();
            load_track_index(idx);
            if (was_playing && track_ready) {
                start_playback();
            } else if (was_paused && track_ready) {
                start_playback();
                pause_playback();
            }
        }

        void set_play_mode(int mode) {
            play_mode = mode;
            update_play_mode_label();
        }

        void cycle_play_mode() {
            set_play_mode((play_mode + 1) % 3);
        }

        void process_input_events() {
            if (!kernel) return;
            const std::size_t count = kernel->input_event_count();
            for (std::size_t i = 0; i < count; ++i) {
                const auto& item = kernel->input_event(i);
                if (item.event.type != Event::Type::MouseUp) continue;
                const auto target = item.target;
                if (target == handles.btn_prev) {
                    switch_track(-1);
                } else if (target == handles.btn_next) {
                    switch_track(1);
                } else if (target == handles.btn_pause) {
                    if (playing) pause_playback();
                    else if (paused) resume_playback();
                    else start_playback();
                } else if (target == handles.btn_mode) {
                    cycle_play_mode();
                }
            }

            if (handles.list) {
                const int selected = kernel->list_view_selected(handles.list);
                if (selected >= 0 && selected != last_list_selected) {
                    last_list_selected = selected;
                    if (!ignore_list_select) {
                        select_track_index(selected);
                    }
                }
            }
        }
    };
}

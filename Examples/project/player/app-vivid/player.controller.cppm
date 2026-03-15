module;
#include <SDL3/SDL.h>
#include <algorithm>
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
import charm.core.geometry;
import charm.core.handle;
import charm.core.soa_kernel;
import charm.core.soa_payload;
import fs_core;
import fs_vfs;
import player.playback;
import player.fs_utils;
import player.storage;
import player.ui;

export namespace player {
    using namespace player::fs_utils;
    using namespace player::ui;

    struct UiHandles {
        WidgetHandle root{};
        WidgetHandle cover{};
        WidgetHandle title{};
        WidgetHandle subtitle{};
        WidgetHandle status{};
        WidgetHandle progress{};
        WidgetHandle time{};
        WidgetHandle spectrum{};
        WidgetHandle eq_panel{};
        WidgetHandle eq_title{};
        std::array<WidgetHandle, kEqBands> eq_labels{};
        std::array<WidgetHandle, kEqBands> eq_sliders{};
        std::array<WidgetHandle, kEqBands> eq_values{};
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
        PlaybackEngine playback{};
        SoaKernel* kernel{nullptr};
        UiHandles handles{};
        PlayerIconIds icons{};
        int last_time_sec{-1};
        std::vector<std::string>* tracks{nullptr};
        std::vector<std::string> track_labels{};
        int track_index{0};
        std::string title_text{};
        std::string subtitle_text{};
        bool fs_ready{false};
        int play_mode{0};
        bool ignore_list_select{false};
        int last_list_selected{-1};
        bool progress_dragging{false};
        int progress_drag_value{0};
        int progress_drag_sec{0};
        std::array<int, kEqBands> last_eq_values{};
        struct TextSlots {
            soa_detail::TextSlotId title{soa_detail::kInvalidTextSlot};
            soa_detail::TextSlotId subtitle{soa_detail::kInvalidTextSlot};
            soa_detail::TextSlotId status{soa_detail::kInvalidTextSlot};
            soa_detail::TextSlotId time{soa_detail::kInvalidTextSlot};
            soa_detail::TextSlotId mode_hint{soa_detail::kInvalidTextSlot};
            soa_detail::TextSlotId list_title{soa_detail::kInvalidTextSlot};
            soa_detail::TextSlotId list_hint{soa_detail::kInvalidTextSlot};
            soa_detail::TextSlotId btn_pause{soa_detail::kInvalidTextSlot};
            std::array<soa_detail::TextSlotId, kEqBands> eq_values{
                soa_detail::kInvalidTextSlot,
                soa_detail::kInvalidTextSlot,
                soa_detail::kInvalidTextSlot,
                soa_detail::kInvalidTextSlot,
                soa_detail::kInvalidTextSlot,
            };
        } text_slots{};
        std::string mount_status{};
        std::mt19937 rng{static_cast<unsigned int>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count())};

        void bind_kernel(SoaKernel& k) {
            kernel = &k;
        }

        void bind_player(audio::AudioPlayer& p) {
            playback.set_player(p);
        }

        const char* track_path() const noexcept {
            return playback.track_path();
        }

        bool track_ready() const noexcept {
            return playback.track_ready();
        }

        void clear_track_state() noexcept {
            playback.set_track_path(nullptr);
            playback.set_track_ready(false);
        }

        void handle_key_action(SDL_Keycode key) {
            switch (key) {
            case SDLK_UP:
                focus_list();
                nav_list(-1);
                break;
            case SDLK_DOWN:
                focus_list();
                nav_list(1);
                break;
            case SDLK_RETURN:
                focus_list();
                nav_list_activate();
                break;
            case SDLK_SPACE:
                if (is_playing()) pause_playback();
                else if (is_paused()) resume_playback();
                else start_playback();
                break;
            case SDLK_N:
                switch_track(1);
                break;
            case SDLK_P:
                switch_track(-1);
                break;
            default:
                break;
            }
        }

        void init_text_slots() {
            if (!kernel) return;
            auto alloc = [this]() noexcept {
                return kernel->alloc_text_slot();
            };
            text_slots.title = alloc();
            text_slots.subtitle = alloc();
            text_slots.status = alloc();
            text_slots.time = alloc();
            text_slots.mode_hint = alloc();
            text_slots.list_title = alloc();
            text_slots.list_hint = alloc();
            text_slots.btn_pause = alloc();
            for (auto& slot : text_slots.eq_values) {
                slot = alloc();
            }
        }

        void set_label(WidgetHandle h, const char* text) {
            if (!kernel || !h) return;
            kernel->set_text(h, text);
        }

        void set_label_slot(WidgetHandle h, soa_detail::TextSlotId slot, const char* text) {
            if (!kernel || !h) return;
            if (slot != soa_detail::kInvalidTextSlot) {
                kernel->set_text_slot(h, slot, text);
                return;
            }
            kernel->set_text(h, text);
        }

        void set_status(const char* text) { set_label_slot(handles.status, text_slots.status, text); }

        bool is_playing() const noexcept { return playback.playing(); }
        bool is_paused() const noexcept { return playback.paused(); }

        void on_player_stopped() {
            playback.stop_playback();
            set_status("Stopped");
            set_play_button_text(false);
        }

        void on_player_error(const char* text) {
            playback.stop_playback();
            set_status(text);
            set_play_button_text(false);
        }

        void tick_player(const audio::AudioPlayer& player) {
            if (playback.playing() && !player.is_running()) {
                if (player.state() == audio::PlayerState::error) {
                    on_player_stopped();
                } else {
                    handle_track_end();
                }
            }
            if (!playback.paused()) {
                const auto st = player.state();
                if (st == audio::PlayerState::opening) {
                    set_status("Opening");
                } else if (st == audio::PlayerState::buffering) {
                    set_status("Buffering");
                } else if (st == audio::PlayerState::playing) {
                    set_status("Playing");
                }
            }
            if (player.state() == audio::PlayerState::error) {
                const auto err = player.last_error();
                const auto stage = player.last_error_stage();
                char buf[96]{};
                std::snprintf(buf, sizeof(buf), "Player error (%s/%s)",
                              player::audio_err_text(err), player::audio_stage_text(stage));
                on_player_error(buf);
            }
            update_duration_from_player();
            update_progress();
        }

        void set_play_button_text(bool playing_now) {
            if (!kernel) return;
            kernel->set_button_icon(handles.btn_pause, playing_now ? icons.pause : icons.play);
            set_label_slot(handles.btn_pause, text_slots.btn_pause, playing_now ? "Pause" : "Play");
        }

        void set_time_label(int elapsed_sec) {
            if (elapsed_sec == last_time_sec) return;
            last_time_sec = elapsed_sec;
            char buf[32]{};
            const int total = playback.duration_sec();
            const int cur_m = elapsed_sec / 60;
            const int cur_s = elapsed_sec % 60;
            const int total_m = total / 60;
            const int total_s = total % 60;
            std::snprintf(buf, sizeof(buf), "%d:%02d / %d:%02d", cur_m, cur_s, total_m, total_s);
            set_label_slot(handles.time, text_slots.time, buf);
        }

        void reset_duration() {
            playback.reset_duration();
            last_time_sec = -1;
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
            set_label_slot(handles.mode_hint, text_slots.mode_hint, buf);
            if (!kernel) return;
            auto icon = icons.loop;
            if (play_mode == 1) icon = icons.single;
            else if (play_mode == 2) icon = icons.shuffle;
            kernel->set_button_icon(handles.btn_mode, icon);
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

        void apply_storage_state(StorageState&& state) {
            fs_ready = state.fs_ready;
            mount_status = std::move(state.mount_status);
            const std::string status = state.status;
            if (tracks) {
                *tracks = std::move(state.tracks);
            }
            if (!status.empty()) {
                set_status(status.c_str());
            }
            rebuild_track_labels();
            refresh_list_view();
            update_list_placeholder();
        }

        void update_list_title() {
            if (!kernel) return;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            char buf[64]{};
            if (count > 0) {
                std::snprintf(buf, sizeof(buf), "Tracks (%d)", count);
                set_label_slot(handles.list_title, text_slots.list_title, buf);
            } else {
                set_label_slot(handles.list_title, text_slots.list_title, "Tracks");
            }
        }

        void update_list_placeholder() {
            if (!kernel || !handles.list_hint) return;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            const bool show = (count == 0);
            kernel->set_visible(handles.list_hint, show);
            if (!show) return;
            if (!mount_status.empty()) {
                set_label_slot(handles.list_hint, text_slots.list_hint, mount_status.c_str());
                return;
            }
            set_label_slot(handles.list_hint, text_slots.list_hint,
                           fs_ready ? "No tracks in /music or /" : "FS not ready");
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
            if (!playback.update_duration_from_player()) return;
            const int dur = playback.duration_sec();
            if (progress_dragging) return;
            const int cur = playback.current_sec();
            if (cur > dur) {
                set_time_label(dur);
                sync_progress_value(100);
            } else {
                set_time_label(cur);
            }
        }

        bool update_progress() {
            if (!kernel || progress_dragging) return false;
            auto upd = playback.update_progress();
            if (!upd.updated) return false;
            kernel->set_value(handles.progress, upd.value);
            set_time_label(upd.current_sec);
            return true;
        }

        void sync_progress_value(int value) {
            if (!kernel) return;
            kernel->set_value(handles.progress, value);
        }

        int progress_value_from_x(int x) const {
            if (!kernel) return 0;
            const Rect r = kernel->world_rect(handles.progress);
            if (r.w <= 1) return 0;
            const int dx = std::clamp(x - r.x, 0, r.w);
            return (dx * 100) / r.w;
        }

        int progress_sec_from_value(int value) const {
            const int duration_sec = playback.duration_sec();
            if (duration_sec <= 0) return 0;
            const int clamped = std::clamp(value, 0, 100);
            return (clamped * duration_sec) / 100;
        }

        struct PendingActions {
            bool prev{false};
            bool next{false};
            bool toggle_play{false};
            bool cycle_mode{false};
            bool seek{false};
            int seek_sec{0};
            bool select{false};
            int select_index{-1};
        };

        void update_progress_drag(int x) {
            progress_drag_value = progress_value_from_x(x);
            progress_drag_sec = progress_sec_from_value(progress_drag_value);
            sync_progress_value(progress_drag_value);
            set_time_label(progress_drag_sec);
        }

        void end_progress_drag(bool apply_seek, PendingActions& actions) {
            if (!progress_dragging) return;
            progress_dragging = false;
            if (apply_seek) {
                actions.seek = true;
                actions.seek_sec = progress_drag_sec;
            }
        }

        void start_playback() {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            std::string status;
            if (!playback.apply_action(PlaybackAction::start, 0, status)) {
                set_status(status.c_str());
                return;
            }
            set_status(status.c_str());
            set_play_button_text(true);
            set_time_label(0);
            sync_progress_value(0);
        }

        void pause_playback() {
            std::string status;
            if (!playback.apply_action(PlaybackAction::pause, 0, status)) return;
            set_status(status.c_str());
            set_play_button_text(false);
        }

        void resume_playback() {
            std::string status;
            if (!playback.apply_action(PlaybackAction::resume, 0, status)) return;
            set_status(status.c_str());
            set_play_button_text(true);
        }

        void stop_playback() {
            std::string status;
            if (playback.apply_action(PlaybackAction::stop, 0, status)) {
                set_status(status.c_str());
            } else {
                set_status("Stopped");
            }
            set_play_button_text(false);
            set_time_label(0);
            sync_progress_value(0);
        }

        void apply_actions(const PendingActions& actions) {
            if (actions.prev) {
                switch_track(-1);
            } else if (actions.next) {
                switch_track(1);
            } else if (actions.select) {
                select_track_index(actions.select_index);
            }

            if (actions.toggle_play) {
                std::string status;
                if (playback.apply_action(PlaybackAction::toggle, 0, status)) {
                    if (!status.empty()) set_status(status.c_str());
                    const bool playing_now = playback.playing();
                    set_play_button_text(playing_now);
                    if (status == "Opening") {
                        set_time_label(0);
                        sync_progress_value(0);
                    }
                } else if (!status.empty()) {
                    set_status(status.c_str());
                }
            }

            if (actions.cycle_mode) {
                cycle_play_mode();
            }

            if (actions.seek) {
                std::string status;
                if (playback.apply_action(PlaybackAction::seek, actions.seek_sec, status)) {
                    if (!status.empty()) set_status(status.c_str());
                } else if (!status.empty()) {
                    set_status(status.c_str());
                }
            }
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

            set_label_slot(handles.title, text_slots.title, title_text.c_str());
            set_label_slot(handles.subtitle, text_slots.subtitle, subtitle_text.c_str());
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
            const char* track_path = vfs_path.c_str();
            set_track_labels(vfs_path);
            fs::File f{};
            auto st = fs::vfs_open(vfs_path, f);
            bool track_ready = false;
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
            playback.set_track_path(track_path);
            playback.set_track_ready(track_ready);
            set_play_button_text(false);
            set_time_label(0);
            sync_progress_value(0);
            reset_duration();
            last_time_sec = -1;
            sync_list_selection();
            return playback.track_ready();
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
            const bool was_active = playback.playing() || playback.paused();
            stop_playback();
            load_track_index(next);
            if (was_active && playback.track_ready()) {
                start_playback();
            }
        }

        void select_track_index(int idx) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            if (!tracks || tracks->empty()) return;
            const bool was_playing = playback.playing();
            const bool was_paused = playback.paused();
            stop_playback();
            load_track_index(idx);
            if (was_playing && playback.track_ready()) {
                start_playback();
            } else if (was_paused && playback.track_ready()) {
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

        void focus_list() {
            if (!kernel || !handles.list) return;
            kernel->set_focused(handles.list, true);
        }

        void nav_list(int delta) {
            if (!kernel || !handles.list) return;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            if (count <= 0) return;
            int selected = kernel->list_view_selected(handles.list);
            if (selected < 0) selected = 0;
            selected += delta;
            if (selected < 0) selected = 0;
            if (selected >= count) selected = count - 1;
            kernel->set_list_view_selected(handles.list, selected);
        }

        void nav_list_activate() {
            if (!kernel || !handles.list) return;
            const int selected = kernel->list_view_selected(handles.list);
            if (selected >= 0) {
                select_track_index(selected);
            }
        }

        void sync_eq_values() {
            if (!kernel) return;
            for (std::size_t i = 0; i < kEqBands; ++i) {
                if (!handles.eq_sliders[i] || !handles.eq_values[i]) continue;
                const int value = kernel->value(handles.eq_sliders[i]);
                if (value == last_eq_values[i]) continue;
                last_eq_values[i] = value;
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%+d", value);
                set_label_slot(handles.eq_values[i], text_slots.eq_values[i], buf);
            }
        }

        void process_input_events() {
            if (!kernel) return;
            PendingActions actions{};
            const std::size_t count = kernel->input_event_count();
            for (std::size_t i = 0; i < count; ++i) {
                const auto& item = kernel->input_event(i);
                const auto target = item.target;
                const auto type = item.event.type;
                if (target == handles.progress) {
                    if (type == Event::Type::MouseDown) {
                        progress_dragging = true;
                        update_progress_drag(item.event.x);
                    } else if (type == Event::Type::DragStart || type == Event::Type::DragMove
                               || type == Event::Type::MouseMove) {
                        if (progress_dragging) {
                            update_progress_drag(item.event.x);
                        }
                    } else if (type == Event::Type::MouseUp || type == Event::Type::DragEnd) {
                        if (progress_dragging) {
                            update_progress_drag(item.event.x);
                            end_progress_drag(true, actions);
                        }
                    } else if (type == Event::Type::Cancel) {
                        end_progress_drag(false, actions);
                    }
                    continue;
                }
                if (type == Event::Type::MouseUp) {
                    if (target == handles.btn_prev) {
                        actions.prev = true;
                    } else if (target == handles.btn_next) {
                        actions.next = true;
                    } else if (target == handles.btn_pause) {
                        actions.toggle_play = true;
                    } else if (target == handles.btn_mode) {
                        actions.cycle_mode = true;
                    }
                }
            }

            if (handles.list) {
                const int selected = kernel->list_view_selected(handles.list);
                if (selected >= 0 && selected != last_list_selected) {
                    last_list_selected = selected;
                    if (!ignore_list_select) {
                        actions.select = true;
                        actions.select_index = selected;
                    }
                }
            }

            apply_actions(actions);
            sync_eq_values();
        }
    };
}

module;
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

export module player.controller;

import player.fixed_string;
import player.mcu_policy;
import audio.eq;
import audio.player;
import audio.result;
import charm.core.event;
import charm.core.geometry;
import charm.core.handle;
import charm.core.soa_kernel;
import charm.core.soa_payload;
import charm.system.clock;
import player.playback;
import player.fs_utils;
import player.storage;
import player.track_probe;
import player.ui;
import player.cover;
import player.font_cache;

inline constexpr bool kPlayerControllerMcuGuard =
    (player::mcu_policy::guard("player.controller uses std::string/std::vector; port before MCU build."), true);

export namespace player {
    using namespace player::fs_utils;
    using namespace player::ui;

    enum class UiKey {
        Up,
        Down,
        Enter,
        PlayToggle,
        Next,
        Prev,
        Mode,
    };

    enum class CoverStrategy : std::uint8_t {
        embedded_first,
        folder_first,
        embedded_only,
        folder_only,
    };

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
        WidgetHandle volume_label{};
        WidgetHandle volume_slider{};
        WidgetHandle volume_value{};
        WidgetHandle dc_label{};
        WidgetHandle dc_switch{};
        WidgetHandle clip_label{};
        WidgetHandle clip_switch{};
        WidgetHandle clip_slider{};
        WidgetHandle clip_value{};
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
        WidgetHandle debug_text{};
    };

    struct PlayerController {
        PlaybackEngine playback{};
        SoaKernel* kernel{nullptr};
        UiHandles handles{};
        PlayerIconIds icons{};
        int last_time_sec{-1};
        StorageView storage{};
        int track_index{0};
        FixedString<192> title_text{};
        FixedString<64> subtitle_text{};
        FixedString<260> cover_path{};
        FixedString<260> cover_embedded_path{};
        FixedString<260> cover_folder_path{};
        FixedString<128> last_status_text{};
        FixedString<48> last_mode_text{};
        FixedString<64> last_list_title_text{};
        FixedString<128> last_list_hint_text{};
        FixedString<128> last_debug_text{};
        CoverImage cover_image{};
        bool cover_ready{false};
        CoverStrategy cover_strategy{CoverStrategy::embedded_first};
        bool fs_ready{false};
        int play_mode{0};
        int last_play_button_state{-1};
        int last_list_count{-1};
        bool ignore_list_select{false};
        int last_list_selected{-1};
        bool progress_dragging{false};
        int progress_drag_value{0};
        int progress_drag_sec{0};
        audio::EqConfig eq_config{};
        std::array<int, kEqBands> eq_values{};
        std::array<int, kEqBands> last_eq_values{};
        int last_volume_value{-1};
        int last_dc_enabled{-1};
        int last_clip_enabled{-1};
        int last_clip_threshold{-1};
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
            soa_detail::TextSlotId volume_value{soa_detail::kInvalidTextSlot};
            soa_detail::TextSlotId clip_value{soa_detail::kInvalidTextSlot};
            soa_detail::TextSlotId debug_text{soa_detail::kInvalidTextSlot};
        } text_slots{};
        FixedString<128> mount_status{};
        std::uint32_t rng_state{0};
        std::uint64_t last_debug_tick_ms{0};
        bool last_running{false};

        static bool is_flac_path(std::string_view path) noexcept {
            const auto dot = path.find_last_of('.');
            if (dot == std::string_view::npos || dot + 1 >= path.size()) return false;
            std::string_view ext = path.substr(dot + 1);
            if (ext.size() != 4) return false;
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
            const char d = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[3])));
            return a == 'f' && b == 'l' && c == 'a' && d == 'c';
        }

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
            reset_cover_image();
        }

        void handle_key_action(UiKey key) {
            switch (key) {
            case UiKey::Up:
                focus_list();
                nav_list(-1);
                break;
            case UiKey::Down:
                focus_list();
                nav_list(1);
                break;
            case UiKey::Enter:
                focus_list();
                nav_list_activate();
                break;
            case UiKey::PlayToggle:
                if (is_playing()) pause_playback();
                else if (is_paused()) resume_playback();
                else start_playback();
                break;
            case UiKey::Next:
                switch_track(1);
                break;
            case UiKey::Prev:
                switch_track(-1);
                break;
            case UiKey::Mode:
                cycle_play_mode();
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
            text_slots.volume_value = alloc();
            text_slots.clip_value = alloc();
            text_slots.debug_text = alloc();
        }

        void set_label(WidgetHandle h, const char* text) {
            if (!kernel || !h) return;
            player::font_cache::ensure_text(text);
            kernel->set_text(h, text);
        }

        void set_label_slot(WidgetHandle h, soa_detail::TextSlotId slot, const char* text) {
            if (!kernel || !h) return;
            player::font_cache::ensure_text(text);
            if (slot != soa_detail::kInvalidTextSlot) {
                kernel->set_text_slot(h, slot, text);
                return;
            }
            kernel->set_text(h, text);
        }

        void set_status(const char* text) {
            const char* value = text ? text : "";
            if (last_status_text.view() == value) return;
            last_status_text.assign(value);
            set_label_slot(handles.status, text_slots.status, value);
        }

        bool is_playing() const noexcept { return playback.playing(); }
        bool is_paused() const noexcept { return playback.paused(); }

        void reset_cover_image() noexcept {
            cover_ready = false;
            cover_path.clear();
            cover_embedded_path.clear();
            cover_folder_path.clear();
            release_cover_image(cover_image);
            if (kernel && handles.cover && kernel->kind(handles.cover) == WidgetKind::Image) {
                kernel->set_image(handles.cover, soa_detail::invalid_image_id());
            }
        }

        void update_cover_image() {
            if (!kernel || !handles.cover) return;
            if (!cover_ready || (cover_embedded_path.empty() && cover_folder_path.empty())) {
                release_cover_image(cover_image);
                if (kernel->kind(handles.cover) == WidgetKind::Image) {
                    kernel->set_image(handles.cover, soa_detail::invalid_image_id());
                }
#if defined(CHARM_PLAYER_COVER_DEBUG)
                std::printf("[cover] no cover for track\n");
#endif
                return;
            }
            auto try_load = [&](std::string_view candidate) -> bool {
                if (candidate.empty()) return false;
                if (cover_image.path == candidate && soa_detail::image_id_valid(cover_image.image_id)) {
                    if (kernel->kind(handles.cover) == WidgetKind::Image) {
                        kernel->set_image(handles.cover, cover_image.image_id);
                    }
                    cover_path.assign(candidate);
                    return true;
                }
                if (load_cover_image(candidate, cover_image)) {
                    if (kernel->kind(handles.cover) == WidgetKind::Image) {
                        kernel->set_image(handles.cover, cover_image.image_id);
                    }
                    cover_path.assign(candidate);
                    return true;
                }
                return false;
            };

            bool loaded = false;
            switch (cover_strategy) {
            case CoverStrategy::embedded_only:
                loaded = try_load(cover_embedded_path.view());
                break;
            case CoverStrategy::folder_only:
                loaded = try_load(cover_folder_path.view());
                break;
            case CoverStrategy::folder_first:
                loaded = try_load(cover_folder_path.view());
                if (!loaded) loaded = try_load(cover_embedded_path.view());
                break;
            case CoverStrategy::embedded_first:
            default:
                loaded = try_load(cover_embedded_path.view());
                if (!loaded) loaded = try_load(cover_folder_path.view());
                break;
            }

            if (loaded) return;
            if (kernel->kind(handles.cover) == WidgetKind::Image) {
                kernel->set_image(handles.cover, soa_detail::invalid_image_id());
            }
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[cover] load failed: %s\n", cover_path.c_str());
#endif
        }

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
            const bool running_now = player.is_running();
            if (last_running && !running_now) {
                if (player.state() == audio::PlayerState::error) {
                    on_player_stopped();
                } else if (playback.playing() || playback.paused()) {
                    handle_track_end();
                }
            }
            last_running = running_now;
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
            update_debug_overlay();
        }

        void set_play_button_text(bool playing_now) {
            if (!kernel) return;
            const int state = playing_now ? 1 : 0;
            if (last_play_button_state == state) return;
            last_play_button_state = state;
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
            if (last_mode_text.view() == buf) return;
            last_mode_text.assign(buf);
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
            const auto* labels = self->storage.track_labels;
            if (!labels) return "";
            if (index >= labels->size()) return "";
            return (*labels)[index].c_str();
        }

        void refresh_track_labels() {
            const auto* labels = storage.track_labels;
            if (!labels) return;
            for (const auto& label : *labels) {
                player::font_cache::ensure_text(label.c_str());
            }
        }

        void sync_list_selection() {
            if (!kernel || !handles.list) return;
            const auto* labels = storage.track_labels;
            if (!labels) return;
            if (track_index < 0 || track_index >= static_cast<int>(labels->size())) return;
            ignore_list_select = true;
            kernel->set_list_view_selected(handles.list, track_index);
            last_list_selected = track_index;
            ignore_list_select = false;
        }

        void refresh_list_view() {
            if (!kernel || !handles.list) return;
            const auto* tracks = storage.tracks;
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

        void apply_storage_view(StorageView view) {
            storage = view;
            fs_ready = storage.fs_ready;
            mount_status.assign(storage.mount_status);
            refresh_track_labels();
            if (!storage.status.empty()) { set_status(storage.status.data()); }
#if defined(CHARM_PLAYER_COVER_DEBUG)
            if (player::font_cache::ready()) {
                set_status("FontCache: OK");
            } else {
                set_status("FontCache: OFF");
            }
#endif
            refresh_list_view();
            update_list_placeholder();
        }

        void update_list_title() {
            if (!kernel) return;
            const auto* tracks = storage.tracks;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            char buf[64]{};
            if (count > 0) {
                std::snprintf(buf, sizeof(buf), "Tracks (%d)", count);
                if (last_list_title_text.view() == buf) return;
                last_list_title_text.assign(buf);
                set_label_slot(handles.list_title, text_slots.list_title, buf);
            } else {
                if (last_list_title_text.view() == "Tracks") return;
                last_list_title_text.assign("Tracks");
                set_label_slot(handles.list_title, text_slots.list_title, "Tracks");
            }
            last_list_count = count;
        }

        void update_list_placeholder() {
            if (!kernel || !handles.list_hint) return;
            const auto* tracks = storage.tracks;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            const bool show = (count == 0);
            kernel->set_visible(handles.list_hint, show);
            if (!show) return;
            if (!mount_status.empty()) {
                if (last_list_hint_text.view() == mount_status.view()) return;
                last_list_hint_text.assign(mount_status.view());
                set_label_slot(handles.list_hint, text_slots.list_hint, mount_status.c_str());
                return;
            }
            const char* hint = fs_ready ? "No tracks in /music or /" : "FS not ready";
            if (last_list_hint_text.view() == hint) return;
            last_list_hint_text.assign(hint);
            set_label_slot(handles.list_hint, text_slots.list_hint, hint);
        }

        int resolve_next_track() {
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->empty()) return -1;
            const int count = static_cast<int>(tracks->size());
            if (play_mode == 1) {
                return track_index;
            }
            if (play_mode == 2) {
                if (count <= 1) return track_index;
                int next = track_index;
                for (int i = 0; i < 4 && next == track_index; ++i) {
                    next = rand_index(count);
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
            const auto* tracks = storage.tracks;
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

        void update_debug_overlay() {
#if CHARM_PLAYER_DEBUG_UI
            if (!kernel || !handles.debug_text) return;
            const auto now_ms = charm::system::ClockCaps::TimeSource::now();
            if (last_debug_tick_ms != 0) {
                const auto dt = now_ms - last_debug_tick_ms;
                if (dt < 500) return;
            }
            last_debug_tick_ms = now_ms;

            audio::PlayerSnapshot snap{};
            if (!playback.snapshot(snap)) return;
            const auto& fmt = snap.output_fmt;
            const std::uint64_t frame_size = fmt.frame_size();
            const std::uint64_t bytes_per_sec = frame_size * fmt.rate;
            const std::uint64_t water_ms = bytes_per_sec ? (snap.water_bytes * 1000 / bytes_per_sec) : 0;
            const std::uint64_t low_ms = bytes_per_sec ? (snap.low_water * 1000 / bytes_per_sec) : 0;
            const std::uint64_t high_ms = bytes_per_sec ? (snap.high_water * 1000 / bytes_per_sec) : 0;

            const auto pump_min_ms = (snap.pump.has_water && bytes_per_sec)
                ? (snap.pump.water_min * 1000 / bytes_per_sec)
                : 0;
            const auto pump_max_ms = (snap.pump.has_water && bytes_per_sec)
                ? (snap.pump.water_max * 1000 / bytes_per_sec)
                : 0;

            char buf[160]{};
            std::snprintf(buf, sizeof(buf),
                          "water %llums (%llu..%llu) pump %llu..%llu underrun %llu/%llu",
                          static_cast<unsigned long long>(water_ms),
                          static_cast<unsigned long long>(low_ms),
                          static_cast<unsigned long long>(high_ms),
                          static_cast<unsigned long long>(pump_min_ms),
                          static_cast<unsigned long long>(pump_max_ms),
                          static_cast<unsigned long long>(snap.stats.underrun_count),
                          static_cast<unsigned long long>(snap.pump.underrun_count));
            if (last_debug_text.view() == buf) return;
            last_debug_text.assign(buf);
            set_label_slot(handles.debug_text, text_slots.debug_text, buf);
#else
            (void)this;
#endif
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
            FixedString<128> status;
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
            FixedString<128> status;
            if (!playback.apply_action(PlaybackAction::pause, 0, status)) return;
            set_status(status.c_str());
            set_play_button_text(false);
        }

        void resume_playback() {
            FixedString<128> status;
            if (!playback.apply_action(PlaybackAction::resume, 0, status)) return;
            set_status(status.c_str());
            set_play_button_text(true);
        }

        void stop_playback() {
            FixedString<128> status;
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
                FixedString<128> status;
                if (playback.apply_action(PlaybackAction::toggle, 0, status)) {
                    if (!status.empty()) set_status(status.c_str());
                    const bool playing_now = playback.playing();
                    set_play_button_text(playing_now);
                    if (status.view() == "Opening") {
                        set_time_label(0);
                        sync_progress_value(0);
                    }
                } else if (!status.empty()) { set_status(status.c_str()); }
            }

            if (actions.cycle_mode) {
                cycle_play_mode();
            }

            if (actions.seek) {
                FixedString<128> status;
                if (playback.apply_action(PlaybackAction::seek, actions.seek_sec, status)) {
                    if (!status.empty()) set_status(status.c_str());
                } else if (!status.empty()) { set_status(status.c_str()); }
            }
        }

        void set_track_labels(int idx) {
            if (!storage.track_titles || !storage.track_subtitles) return;
            if (idx < 0 || idx >= static_cast<int>(storage.track_titles->size())) return;
            title_text.assign((*storage.track_titles)[idx]);
            subtitle_text.assign((*storage.track_subtitles)[idx]);
            set_label_slot(handles.title, text_slots.title, title_text.c_str());
            set_label_slot(handles.subtitle, text_slots.subtitle, subtitle_text.c_str());
        }
        bool load_track_index(int idx) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return false;
            }
            const auto* tracks = storage.tracks;
            const auto* labels = storage.track_labels;
            if (!tracks || tracks->empty()) return false;
            if (!labels) return false;
            if (idx < 0) idx = 0;
            if (idx >= static_cast<int>(tracks->size())) idx = static_cast<int>(tracks->size()) - 1;
            track_index = idx;
            const auto& vfs_path = (*tracks)[track_index];
            const char* track_path = vfs_path.c_str();
            set_track_labels(track_index);
            FixedString<128> status;
            const bool track_ready = player::check_track_ready(vfs_path, status);
            if (!status.empty()) { set_status(status.c_str()); }
            playback.set_track_path(track_path);
            playback.set_track_ready(track_ready);
            if (track_ready) {
                cover_embedded_path.assign(vfs_path);
                cover_folder_path.clear();
                std::string folder_path;
                const bool has_folder = fs_utils::find_cover_for_track(vfs_path, folder_path);
                cover_folder_path.assign(folder_path);
                cover_ready = true;
                switch (cover_strategy) {
                case CoverStrategy::embedded_only:
                    cover_path.assign(cover_embedded_path.c_str());
                    break;
                case CoverStrategy::folder_only:
                    cover_path.assign(has_folder ? cover_folder_path.c_str() : "");
                    cover_ready = has_folder;
                    break;
                case CoverStrategy::folder_first:
                    cover_path.assign(has_folder ? cover_folder_path.c_str() : cover_embedded_path.c_str());
                    break;
                case CoverStrategy::embedded_first:
                default:
                    cover_path.assign(cover_embedded_path.c_str());
                    break;
                }
            } else {
                cover_ready = false;
                cover_path.clear();
                cover_embedded_path.clear();
                cover_folder_path.clear();
            }
            update_cover_image();
            reset_duration();
            if (track_ready) {
                int secs = 0;
                if (player::probe_duration_seconds(track_path, secs)) {
                    playback.set_duration_from_probe(secs);
                }
            }
            set_play_button_text(false);
            set_time_label(0);
            sync_progress_value(0);
            last_time_sec = -1;
            sync_list_selection();
            return playback.track_ready();
        }

        void switch_track(int delta) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            const auto* tracks = storage.tracks;
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
            const auto* tracks = storage.tracks;
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
            const auto* tracks = storage.tracks;
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
            bool changed = false;
            for (std::size_t i = 0; i < kEqBands; ++i) {
                if (!handles.eq_sliders[i] || !handles.eq_values[i]) continue;
                const int value = kernel->value(handles.eq_sliders[i]);
                if (value == last_eq_values[i]) continue;
                last_eq_values[i] = value;
                eq_values[i] = value;
                changed = true;
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%+d", value);
                set_label_slot(handles.eq_values[i], text_slots.eq_values[i], buf);
            }
            if (!changed) return;
            static constexpr std::array<std::uint32_t, kEqBands> kEqFreqs{
                60, 250, 1000, 4000, 16000
            };
            eq_config.enabled = true;
            eq_config.band_count = static_cast<std::uint8_t>(kEqBands);
            for (std::size_t i = 0; i < kEqBands; ++i) {
                eq_config.bands[i].freq_hz = kEqFreqs[i];
                eq_config.bands[i].gain_db = static_cast<float>(eq_values[i]);
                eq_config.bands[i].q = 1.0f;
            }
            FixedString<128> status;
            if (!playback.set_eq(eq_config, status) && !status.empty()) {
                set_status(status.c_str());
            }
        }

        void sync_volume_value() {
            if (!kernel || !handles.volume_slider || !handles.volume_value) return;
            const int value = kernel->value(handles.volume_slider);
            if (value == last_volume_value) return;
            last_volume_value = value;
            char buf[16]{};
            std::snprintf(buf, sizeof(buf), "%d", value);
            set_label_slot(handles.volume_value, text_slots.volume_value, buf);
            FixedString<128> status;
            if (!playback.set_volume(value, status) && !status.empty()) {
                set_status(status.c_str());
            }
        }

        void sync_dsp_controls() {
            if (!kernel) return;
            if (handles.dc_switch) {
                const int enabled = kernel->checked(handles.dc_switch) ? 1 : 0;
                if (enabled != last_dc_enabled) {
                    last_dc_enabled = enabled;
                    FixedString<128> status;
                    if (!playback.set_dc_block(enabled != 0, status) && !status.empty()) {
                        set_status(status.c_str());
                    }
                }
            }
            if (!handles.clip_switch || !handles.clip_slider || !handles.clip_value) return;
            const int enabled = kernel->checked(handles.clip_switch) ? 1 : 0;
            const int threshold = kernel->value(handles.clip_slider);
            bool changed = false;
            if (enabled != last_clip_enabled) {
                last_clip_enabled = enabled;
                changed = true;
            }
            if (threshold != last_clip_threshold) {
                last_clip_threshold = threshold;
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%d", threshold);
                set_label_slot(handles.clip_value, text_slots.clip_value, buf);
                changed = true;
            }
            if (!changed) return;
            FixedString<128> status;
            if (!playback.set_soft_clip(enabled != 0, threshold, status) && !status.empty()) {
                set_status(status.c_str());
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
            sync_volume_value();
            sync_dsp_controls();
        }

        void seed_rng() {
            if (rng_state != 0) return;
            auto seed = static_cast<std::uint32_t>(charm::system::ClockCaps::TimeSource::now());
            if (seed == 0) seed = 0xA341316Cu;
            rng_state = seed;
        }

        std::uint32_t next_rng() {
            seed_rng();
            std::uint32_t x = rng_state;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            rng_state = x;
            return x;
        }

        int rand_index(int max) {
            if (max <= 0) return 0;
            return static_cast<int>(next_rng() % static_cast<std::uint32_t>(max));
        }
    };
}

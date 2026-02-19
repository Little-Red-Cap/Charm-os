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
import charm.core.factory;
import charm.core.handle;
import charm.gfx.color;
import charm.widgets.button;
import charm.widgets.chart;
import charm.widgets.histogram_view;
import charm.widgets.label;
import charm.widgets.list_view;
import charm.widgets.progress;
import charm.widgets.scrollbar;
import charm.widgets.switcher;
import charm.widgets.slider;
import charm.widgets.dropdown;
#if CHARM_PLAYER_DEBUG_UI
import charm.widgets.menu_tree;
#endif
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
        WidgetHandle list{};
        WidgetHandle list_title{};
        WidgetHandle list_hint{};
        WidgetHandle list_scroll{};
        WidgetHandle mode_hint{};
        WidgetHandle spectrum_hist{};
        WidgetHandle spectrum_peak{};
        WidgetHandle options_row{};
        WidgetHandle opt_spectrum_label{};
        WidgetHandle opt_spectrum_switch{};
        WidgetHandle opt_low_label{};
        WidgetHandle opt_low_switch{};
        WidgetHandle opt_eq_label{};
        WidgetHandle opt_eq_switch{};
        WidgetHandle eq_panel{};
        WidgetHandle eq_title{};
        WidgetHandle eq_preset_label{};
        WidgetHandle eq_preset{};
        std::array<WidgetHandle, kEqBands> eq_band_labels{};
        std::array<WidgetHandle, kEqBands> eq_sliders{};
        std::array<WidgetHandle, kEqBands> eq_value_labels{};
#if CHARM_PLAYER_DEBUG_UI
        WidgetHandle table{};
        WidgetHandle tree{};
        WidgetHandle debug_grid{};
        WidgetHandle chart{};
        WidgetHandle debug_side{};
        WidgetHandle logo{};
        WidgetHandle stepper{};
        WidgetHandle timeline{};
        WidgetHandle rich_text{};
        WidgetHandle code_block{};
        WidgetHandle progress_wheel{};
        WidgetHandle waveform{};
        WidgetHandle battery_gauge{};
        WidgetHandle histogram{};
        WidgetHandle fold_panel{};
        WidgetHandle progress_flow{};
        WidgetHandle cloudy_glass{};
        WidgetHandle spinning_wheel{};
        WidgetHandle image_box{};
        WidgetHandle meter_pointer{};
        WidgetHandle progress_drill{};
#endif
        WidgetHandle progress{};
        WidgetHandle time{};
        WidgetHandle btn_prev{};
        WidgetHandle btn_pause{};
        WidgetHandle btn_next{};
        WidgetHandle btn_mode{};
        WidgetHandle controls{};
        WidgetHandle perf_overlay{};
    };

    struct PlayerController {
        audio::AudioPlayer* player{nullptr};
        UiFactory* factory{nullptr};
        UiHandles handles{};
        bool playing{false};
        bool track_ready{false};
        bool dragging{false};
        std::chrono::steady_clock::time_point start{};
        int duration_sec{180};
        int current_sec{0};
        int drag_origin_sec{0};
        int drag_target_sec{0};
        int pending_seek_sec{-1};
        const char* track_path{nullptr};
        std::vector<std::string>* tracks{nullptr};
        std::vector<std::string> track_labels{};
        int track_index{0};
        std::string title_text{};
        std::string subtitle_text{};
        bool paused{false};
        bool updating{false};
        bool fs_ready{false};
        int play_mode{0};
        bool spectrum_enabled{true};
        bool spectrum_low_load{false};
        bool eq_enabled{false};
        bool eq_ui_guard{false};
        int eq_preset_index{0};
        std::array<int, kEqBands> eq_gains{};
        std::uint32_t spectrum_tick{0};
        bool duration_ready{false};
        bool ignore_list_select{false};
        std::string mount_status{};
        bool syncing_scrollbar{false};
        std::array<float, audio::AudioPlayer::spectrum_bins> spectrum_values{};
        std::array<float, audio::AudioPlayer::spectrum_bins> spectrum_bars{};
        std::array<float, audio::AudioPlayer::spectrum_bins> spectrum_peaks{};
        std::array<int, audio::AudioPlayer::spectrum_bins> spectrum_bar_points{};
        std::array<int, audio::AudioPlayer::spectrum_bins> spectrum_peak_points{};
        std::mt19937 rng{static_cast<unsigned int>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count())};
#if CHARM_PLAYER_DEBUG_UI
        bool show_debug{false};
        MenuTree menu_tree{};
#endif
        struct ListCacheEntry {
            int index{-1};
            int width{0};
            std::string text{};
        };
        std::array<ListCacheEntry, 32> list_cache{};

        void set_label(WidgetHandle h, const char* text) {
            if (!factory) return;
            if (auto* label = factory->get_label(h)) {
                label->set_text(text);
            }
        }

        void set_status(const char* text) { set_label(handles.status, text); }

        void set_cover_color(const rgba& color) {
            if (!factory) return;
            if (auto* cover = factory->get_container(handles.cover)) {
                cover->set_background(color);
            }
        }

        void set_status_color(const rgba& color) {
            if (!factory) return;
            if (auto* label = factory->get_label(handles.status)) {
                label->set_color(color);
            }
        }

        void set_play_button_icon(bool playing_now) {
            if (!factory) return;
            if (auto* btn = factory->get_button(handles.btn_pause)) {
                btn->set_text("");
                btn->set_icon(playing_now ? icon_pause() : icon_play(), 20, 20);
            }
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

        void update_play_mode_icon() {
            if (!factory) return;
            if (auto* btn = factory->get_button(handles.btn_mode)) {
                btn->set_text("");
                if (play_mode == 1) {
                    btn->set_icon(icon_single(), 18, 18);
                } else if (play_mode == 2) {
                    btn->set_icon(icon_shuffle(), 18, 18);
                } else {
                    btn->set_icon(icon_loop(), 18, 18);
                }
            }
        }

        void update_eq_label() {
            if (!factory) return;
            auto* label = factory->get_label(handles.opt_eq_label);
            if (!label) return;
            label->set_text(eq_enabled ? "EQ On" : "EQ Off");
        }

        void update_eq_panel_labels() {
            if (!factory) return;
            for (std::size_t i = 0; i < eq_gains.size(); ++i) {
                auto* label = factory->get_label(handles.eq_value_labels[i]);
                if (!label) continue;
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%+d dB", eq_gains[i]);
                label->set_text(buf);
            }
        }

        void update_eq_panel_state() {
            if (!factory) return;
            const bool show = eq_enabled && !spectrum_low_load;
            if (auto* panel = factory->get_container(handles.eq_panel)) {
                panel->set_visible(show);
            }
            if (auto* sw = factory->get_switch(handles.opt_eq_switch)) {
                sw->set_enabled(!spectrum_low_load);
            }
            if (auto* dropdown = factory->get_dropdown(handles.eq_preset)) {
                dropdown->set_enabled(show);
            }
            for (auto h : handles.eq_sliders) {
                if (auto* slider = factory->get_slider(h)) {
                    slider->set_enabled(show);
                }
            }
        }

        void apply_eq_to_player() {
            if (!player) return;
            audio::EqConfig cfg{};
            cfg.enabled = eq_enabled && !spectrum_low_load;
            cfg.band_count = static_cast<std::uint8_t>(eq_gains.size());
            for (std::size_t i = 0; i < eq_gains.size(); ++i) {
                cfg.bands[i].freq_hz = kEqFrequencies[i];
                cfg.bands[i].gain_db = static_cast<float>(eq_gains[i]);
                cfg.bands[i].q = 1.0f;
            }
            (void)player->set_eq(cfg);
        }

        static constexpr int kPresetFlat = 0;
        static constexpr int kPresetBass = 1;
        static constexpr int kPresetVocal = 2;
        static constexpr int kPresetTreble = 3;
        static constexpr int kPresetCustom = 4;
        static constexpr std::array<std::uint32_t, kEqBands> kEqFrequencies = {60, 250, 1000, 4000, 12000};

        void update_eq_sliders_from_gains() {
            if (!factory) return;
            for (std::size_t i = 0; i < eq_gains.size(); ++i) {
                auto* slider = factory->get_slider(handles.eq_sliders[i]);
                if (!slider) continue;
                slider->set_value(eq_gains[i]);
            }
        }

        void apply_eq_preset(int preset) {
            static constexpr int flat[kEqBands] = {0, 0, 0, 0, 0};
            static constexpr int bass[kEqBands] = {6, 4, 0, -2, -3};
            static constexpr int vocal[kEqBands] = {-2, 0, 4, 3, -1};
            static constexpr int treble[kEqBands] = {-3, -1, 1, 4, 6};
            const int* src = nullptr;
            switch (preset) {
            case kPresetFlat: src = flat; break;
            case kPresetBass: src = bass; break;
            case kPresetVocal: src = vocal; break;
            case kPresetTreble: src = treble; break;
            default: return;
            }
            for (std::size_t i = 0; i < eq_gains.size(); ++i) {
                eq_gains[i] = src[i];
            }
            update_eq_sliders_from_gains();
            update_eq_panel_labels();
            apply_eq_to_player();
        }

        void update_low_load_label() {
            if (!factory) return;
            auto* label = factory->get_label(handles.opt_low_label);
            if (!label) return;
            label->set_text(spectrum_low_load ? "Low load On" : "Low load");
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
            if (!factory) return;
            auto* list = factory->get_list_view(handles.list);
            if (!list) return;
            if (track_index < 0 || track_index >= static_cast<int>(track_labels.size())) return;
            ignore_list_select = true;
            list->set_selected(track_index);
            ignore_list_select = false;
        }

        void refresh_list_view() {
            if (!factory) return;
            auto* list = factory->get_list_view(handles.list);
            if (!list) return;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            list->set_item_count(count);
            list->set_row_height(32);
            list->set_wheel_step(32);
            if (count > 0) {
                sync_list_selection();
            }
            update_list_title();
            update_list_placeholder();
        }

        void update_list_title() {
            if (!factory) return;
            auto* label = factory->get_label(handles.list_title);
            if (!label) return;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            if (count > 0) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Tracks (%d)", count);
                label->set_text(buf);
            } else {
                label->set_text("Tracks");
            }
        }

        void update_list_placeholder() {
            if (!factory) return;
            auto* label = factory->get_label(handles.list_hint);
            if (!label) return;
#if CHARM_PLAYER_DEBUG_UI
            if (show_debug) {
                label->set_visible(false);
                return;
            }
#endif
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            if (count > 0) {
                label->set_visible(false);
                return;
            }
            label->set_visible(true);
            if (!mount_status.empty()) {
                label->set_text(mount_status.c_str());
                return;
            }
            label->set_text(fs_ready ? "No tracks in /music or /" : "FS not ready");
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

        void set_debug_visible(bool on) {
#if CHARM_PLAYER_DEBUG_UI
            show_debug = on;
            if (!factory) return;
            if (auto* title = factory->get_label(handles.list_title)) {
                title->set_visible(!on);
            }
            if (auto* hint = factory->get_label(handles.list_hint)) {
                hint->set_visible(!on);
            }
            if (auto* hint = factory->get_label(handles.mode_hint)) {
                hint->set_visible(!on);
            }
            if (auto* btn = factory->get_button(handles.btn_mode)) {
                btn->set_visible(!on);
            }
            if (auto* hist = factory->get_histogram_view(handles.spectrum_hist)) {
                hist->set_visible(!on);
            }
            if (auto* chart = factory->get_chart(handles.spectrum_peak)) {
                chart->set_visible(!on);
            }
            if (auto* row = factory->get_container(handles.options_row)) {
                row->set_visible(!on);
            }
            if (auto* list = factory->get_list_view(handles.list)) {
                list->set_visible(!on);
            }
            if (auto* bar = factory->get_scroll_bar(handles.list_scroll)) {
                bar->set_visible(!on);
            }
            if (auto* grid = factory->get_container(handles.debug_grid)) {
                grid->set_visible(on);
            }
#else
            (void)on;
#endif
        }

        void toggle_debug_view() {
#if CHARM_PLAYER_DEBUG_UI
            set_debug_visible(!show_debug);
#endif
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
            if (!playing || updating) return false;
            const auto now = std::chrono::steady_clock::now();
            const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - start).count());
            const int clamped = (elapsed > duration_sec) ? duration_sec : elapsed;
            const int value = (duration_sec > 0)
                ? static_cast<int>((clamped * 100) / duration_sec)
                : 0;

            if (auto* bar = factory->get_progress(handles.progress)) {
                bar->set_value(value);
            }
#if CHARM_PLAYER_DEBUG_UI
            if (auto* meter = factory->get_meter_pointer(handles.meter_pointer)) {
                meter->set_value(value);
            }
            if (auto* drill = factory->get_progress_bar_drill(handles.progress_drill)) {
                drill->set_value(value);
            }
#endif
            set_time_label(clamped);
            return true;
        }

        bool update_spectrum() {
            if (!player || !factory) return false;
            if (!spectrum_enabled) return false;
            if (spectrum_low_load) {
                if ((spectrum_tick++ & 3u) != 0u) return false;
            } else {
                spectrum_tick = 0;
            }
            if (!player->read_spectrum(spectrum_values)) return false;
            constexpr float kBarDecay = 2.0f;
            constexpr float kPeakDecay = 1.0f;
            for (std::size_t i = 0; i < spectrum_values.size(); ++i) {
                const float target = spectrum_values[i] * 100.0f;
                float bar = spectrum_bars[i];
                if (target > bar) bar = target;
                else bar = (bar > kBarDecay) ? (bar - kBarDecay) : 0.0f;
                spectrum_bars[i] = bar;

                float peak = spectrum_peaks[i];
                if (bar > peak) peak = bar;
                else peak = (peak > kPeakDecay) ? (peak - kPeakDecay) : 0.0f;
                spectrum_peaks[i] = peak;

                spectrum_bar_points[i] = static_cast<int>(bar + 0.5f);
                spectrum_peak_points[i] = static_cast<int>(peak + 0.5f);
            }
            if (auto* hist = factory->get_histogram_view(handles.spectrum_hist)) {
                hist->set_values(spectrum_bar_points.data(),
                                 static_cast<int>(spectrum_bar_points.size()));
                hist->set_range(0, 100);
            }
            if (auto* chart = factory->get_chart(handles.spectrum_peak)) {
                chart->set_points(spectrum_peak_points.data(),
                                  static_cast<int>(spectrum_peak_points.size()));
            }
            return true;
        }

        void set_spectrum_enabled(bool on) {
            spectrum_enabled = on;
            if (player) player->enable_spectrum(on);
            if (factory) {
                if (auto* hist = factory->get_histogram_view(handles.spectrum_hist)) {
                    hist->set_visible(on);
                }
                if (auto* chart = factory->get_chart(handles.spectrum_peak)) {
                    chart->set_visible(on);
                }
                if (auto* sw = factory->get_switch(handles.opt_spectrum_switch)) {
                    sw->set_on(on);
                }
            }
        }

        void set_low_load(bool on) {
            spectrum_low_load = on;
            if (factory) {
                if (auto* sw = factory->get_switch(handles.opt_low_switch)) {
                    sw->set_on(on);
                }
                update_low_load_label();
            }
            update_eq_panel_state();
            apply_eq_to_player();
        }

        void set_eq_enabled(bool on) {
            eq_enabled = on;
            if (factory) {
                if (auto* sw = factory->get_switch(handles.opt_eq_switch)) {
                    sw->set_on(on);
                }
                update_eq_label();
            }
            update_eq_panel_state();
            apply_eq_to_player();
        }

        void set_play_mode(int mode) {
            play_mode = mode;
            update_play_mode_label();
            update_play_mode_icon();
        }

        void cycle_play_mode() {
            set_play_mode((play_mode + 1) % 3);
        }

        void sync_option_states() {
            if (factory) {
                if (auto* sw = factory->get_switch(handles.opt_spectrum_switch)) {
                    sw->set_on(spectrum_enabled);
                }
                if (auto* sw = factory->get_switch(handles.opt_low_switch)) {
                    sw->set_on(spectrum_low_load);
                }
                if (auto* sw = factory->get_switch(handles.opt_eq_switch)) {
                    sw->set_on(eq_enabled);
                }
                if (auto* dropdown = factory->get_dropdown(handles.eq_preset)) {
                    eq_ui_guard = true;
                    dropdown->set_selected(eq_preset_index);
                    eq_ui_guard = false;
                }
            }
            update_play_mode_label();
            update_play_mode_icon();
            update_low_load_label();
            update_eq_label();
            update_eq_panel_labels();
            update_eq_panel_state();
        }

        void on_spectrum_toggle() {
            if (!factory) return;
            auto* sw = factory->get_switch(handles.opt_spectrum_switch);
            if (!sw) return;
            set_spectrum_enabled(sw->is_on());
        }

        void on_low_load_toggle() {
            if (!factory) return;
            auto* sw = factory->get_switch(handles.opt_low_switch);
            if (!sw) return;
            set_low_load(sw->is_on());
        }

        void on_eq_toggle() {
            if (!factory) return;
            auto* sw = factory->get_switch(handles.opt_eq_switch);
            if (!sw) return;
            set_eq_enabled(sw->is_on());
        }

        void on_eq_slider_change() {
            if (!factory) return;
            if (eq_ui_guard) return;
            eq_ui_guard = true;
            for (std::size_t i = 0; i < eq_gains.size(); ++i) {
                auto* slider = factory->get_slider(handles.eq_sliders[i]);
                if (!slider) continue;
                eq_gains[i] = slider->value();
            }
            update_eq_panel_labels();
            if (auto* dropdown = factory->get_dropdown(handles.eq_preset)) {
                if (dropdown->selected() != kPresetCustom) {
                    dropdown->set_selected(kPresetCustom);
                    eq_preset_index = kPresetCustom;
                }
            }
            apply_eq_to_player();
            eq_ui_guard = false;
        }

        void on_eq_preset_change() {
            if (!factory) return;
            if (eq_ui_guard) return;
            auto* dropdown = factory->get_dropdown(handles.eq_preset);
            if (!dropdown) return;
            const int preset = dropdown->selected();
            eq_preset_index = preset;
            if (preset == kPresetCustom) return;
            eq_ui_guard = true;
            apply_eq_preset(preset);
            eq_ui_guard = false;
        }

        void sync_progress_value(int value) {
            if (auto* bar = factory->get_progress(handles.progress)) {
                bar->set_value(value);
            }
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

        void apply_pending_seek() {
            if (pending_seek_sec < 0) return;
            if (!is_seek_ready()) return;
            const int target = pending_seek_sec;
            pending_seek_sec = -1;
            if (request_seek(target)) {
                start = std::chrono::steady_clock::now() - std::chrono::seconds(target);
                set_time_label(target);
                const int value = (duration_sec > 0) ? static_cast<int>((target * 100) / duration_sec) : 0;
                sync_progress_value(value);
            }
        }

        void start_playback() {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                set_status_color(kUiError);
                return;
            }
            if (!player || !track_path) {
                set_status("No track");
                set_status_color(kUiError);
                return;
            }
            if (!track_ready) {
                set_status("Track not ready");
                set_status_color(kUiError);
                return;
            }
            (void)player->stop();
            auto res = player->play(track_path);
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Play failed (%s)", audio_err_text(res.error().code));
                set_status(buf);
                return;
            }
            playing = true;
            paused = false;
            start = std::chrono::steady_clock::now();
            set_status("Opening");
            set_status_color(kUiStatus);
            set_play_button_icon(true);
            set_time_label(0);
            sync_progress_value(0);
            apply_pending_seek();
        }

        void pause_playback() {
            if (!player || !playing) return;
            auto res = player->pause();
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Pause failed (%s)", audio_err_text(res.error().code));
                set_status(buf);
                return;
            }
            playing = false;
            paused = true;
            set_status("Paused");
            set_status_color(kUiPaused);
            set_play_button_icon(false);
        }

        void resume_playback() {
            if (!player || !paused) return;
            auto res = player->resume();
            if (!res) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "Resume failed (%s)", audio_err_text(res.error().code));
                set_status(buf);
                return;
            }
            paused = false;
            playing = true;
            start = std::chrono::steady_clock::now() - std::chrono::seconds(current_sec);
            set_status("Playing");
            set_status_color(kUiOk);
            set_play_button_icon(true);
        }

        void stop_playback() {
            if (player) {
                (void)player->stop();
            }
            playing = false;
            paused = false;
            pending_seek_sec = -1;
            set_status("Stopped");
            set_status_color(kUiStatus);
            set_play_button_icon(false);
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
                set_status_color(kUiError);
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
            set_status_color(track_ready ? kUiOk : kUiError);
            set_play_button_icon(false);
            set_time_label(0);
            sync_progress_value(0);
            pending_seek_sec = -1;
            playing = false;
            paused = false;
            reset_duration();
            sync_list_selection();
            return track_ready;
        }

        void switch_track(int delta) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                set_status_color(kUiError);
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
                set_status_color(kUiError);
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

        bool begin_drag(int x, int y) {
            if (!factory) return false;
            auto* bar = factory->get_progress(handles.progress);
            if (!bar) return false;
            const auto r = bar->get_rect();
            if (x < r.x || x >= r.x + r.w || y < r.y || y >= r.y + r.h) return false;
            drag_origin_sec = current_sec;
            dragging = true;
            updating = true;
            return update_drag(x);
        }

        bool update_drag(int x) {
            if (!factory) return false;
            auto* bar = factory->get_progress(handles.progress);
            if (!bar) return false;
            const auto r = bar->get_rect();
            if (r.w <= 0) return false;
            int clamped = x;
            if (clamped < r.x) clamped = r.x;
            if (clamped > r.x + r.w) clamped = r.x + r.w;
            const int value = static_cast<int>(((clamped - r.x) * 100) / r.w);
            bar->set_value(value);
            drag_target_sec = (duration_sec > 0) ? static_cast<int>((value * duration_sec) / 100) : 0;
            set_time_label(drag_target_sec);
            return true;
        }

        void end_drag() {
            if (!dragging) return;
            dragging = false;
            updating = false;
            if (!playing) {
                set_time_label(drag_origin_sec);
                const int value = (duration_sec > 0) ? static_cast<int>((drag_origin_sec * 100) / duration_sec) : 0;
                sync_progress_value(value);
                set_status("Seek only during play");
                return;
            }
            if (is_seek_ready()) {
                if (request_seek(drag_target_sec)) {
                    start = std::chrono::steady_clock::now() - std::chrono::seconds(drag_target_sec);
                } else {
                    set_time_label(drag_origin_sec);
                    const int value = (duration_sec > 0) ? static_cast<int>((drag_origin_sec * 100) / duration_sec) : 0;
                    sync_progress_value(value);
                }
            } else {
                pending_seek_sec = drag_target_sec;
                set_status("Seek queued");
            }
        }
    };
}

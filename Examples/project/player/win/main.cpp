#ifndef CHARM_PLAYER_DEBUG_UI
#define CHARM_PLAYER_DEBUG_UI 0
#endif

import audio.player;
import audio.result;
import player.fs_utils;
import player.ui;
import charm.core.config;
import charm.core.container;
import charm.core.event;
import charm.core.factory;
import charm.core.gui;
import charm.core.layout;
import charm.core.style;
import charm.gfx.canvas;
import charm.gfx.assets.render;
import charm.gfx.framebuffer;
import charm.gfx.color;
import charm.widgets.button;
import charm.widgets.label;
import charm.widgets.list_view;
import charm.widgets.progress;
import charm.widgets.scrollbar;
import charm.widgets.segmented_control;
import charm.widgets.chart;
import charm.widgets.perf_overlay;
import charm.widgets.ring_indication;
import charm.widgets.text_box;
#if CHARM_PLAYER_DEBUG_UI
import charm.widgets.stepper;
import charm.widgets.timeline;
import charm.widgets.menu_tree;
import charm.widgets.rich_text;
import charm.widgets.code_block;
import charm.widgets.table_view;
import charm.widgets.tree_view;
import charm.widgets.progress_wheel;
import charm.widgets.waveform_view;
import charm.widgets.battery_gauge;
import charm.widgets.histogram_view;
import charm.widgets.foldable_panel;
import charm.widgets.progress_flowing;
import charm.widgets.cloudy_glass;
#endif
import charm.widgets.image;
import charm.widgets.text;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import util.core;

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#include <SDL3/SDL.h>
#if defined(_WIN32)
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#endif

#include <array>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
    constexpr const char* kDefaultVhdPath = "G:/Project/dev.vhd";
    using namespace player::fs_utils;
    using namespace player::ui;

    static DefaultFrameBuffer g_framebuffer{};
    static DefaultCanvas g_canvas(g_framebuffer);
    static UiFactory g_factory{};
    static audio::PlayerConfig g_player_cfg{};
    static audio::AudioPlayer g_player(g_player_cfg);
    static std::vector<std::string> g_vfs_tracks{};
    

    const char* audio_err_text(audio::Errc err) {
        switch (err) {
        case audio::Errc::ok: return "ok";
        case audio::Errc::invalid_arg: return "invalid_arg";
        case audio::Errc::not_supported: return "not_supported";
        case audio::Errc::io_error: return "io_error";
        case audio::Errc::decode_error: return "decode_error";
        case audio::Errc::bad_state: return "bad_state";
        case audio::Errc::timeout: return "timeout";
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

#if CHARM_PLAYER_DEBUG_UI
    struct TreeDemoNode {
        const char* label{nullptr};
        int depth{0};
        bool expanded{false};
        bool has_children{false};
    };

    struct TreeCacheEntry {
        int index{-1};
        int depth{0};
        std::string label{};
    };

    struct TreeDemo {
        static constexpr int kMaxNodes = 8;
        std::array<TreeDemoNode, kMaxNodes> nodes{};
        std::array<int, kMaxNodes> visible{};
        std::array<TreeCacheEntry, 32> cache{};
        int node_count{0};
        int visible_count{0};
    };

    TreeDemo g_tree_demo{
        .nodes = {{
            {"System", 0, true, true},
            {"Audio", 1, true, true},
            {"Player", 2, false, false},
            {"Codec", 2, false, false},
            {"UI", 1, true, true},
            {"Vivid", 2, false, false},
            {"Ink", 2, false, false},
            {"Boot", 0, false, false},
        }},
        .node_count = 8,
        .visible_count = 0,
    };

    void tree_rebuild_visible(TreeDemo& demo) noexcept {
        demo.visible_count = 0;
        std::array<bool, 8> open{};
        open.fill(true);
        for (int i = 0; i < demo.node_count; ++i) {
            const auto& node = demo.nodes[i];
            if (node.depth > 0 && !open[static_cast<std::size_t>(node.depth - 1)]) continue;
            demo.visible[demo.visible_count++] = i;
            open[static_cast<std::size_t>(node.depth)] = node.expanded || !node.has_children;
        }
    }

    int tree_row_count(void* ctx) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        return demo ? demo->visible_count : 0;
    }

    TreeView::NodeInfo tree_node_info(void* ctx, int index) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo || index < 0 || index >= demo->visible_count) return {};
        const int node_index = demo->visible[index];
        const auto& node = demo->nodes[node_index];
        return TreeView::NodeInfo{node.depth, node.expanded, node.has_children, node.label};
    }

    void on_tree_draw(void* ctx, DefaultCanvas& cvs, const TreeView::DrawInfo& info) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        const auto& st = Theme::instance().get<TreeView>();
        Rect text = info.rect;
        const char* label = info.node.label ? info.node.label : "";
        int depth = info.node.depth;
        if (demo && info.slot >= 0 && info.slot < static_cast<int>(demo->cache.size())) {
            const auto& entry = demo->cache[info.slot];
            if (entry.index == info.index && !entry.label.empty()) {
                label = entry.label.c_str();
                depth = entry.depth;
            }
        }
        text.x += st.padding + depth * 12;
        text.w -= st.padding * 2;
        if (text.w <= 0 || text.h <= 0) return;
        draw_text_box(cvs, text, label,
                      st.font_color, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    }

    void on_tree_toggle(void* ctx, int index) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo || index < 0 || index >= demo->visible_count) return;
        const int node_index = demo->visible[index];
        auto& node = demo->nodes[node_index];
        if (!node.has_children) return;
        node.expanded = !node.expanded;
        tree_rebuild_visible(*demo);
    }

    void on_tree_pool_create(void* ctx, int slot) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo) return;
        if (slot < 0 || slot >= static_cast<int>(demo->cache.size())) return;
        demo->cache[slot].index = -1;
        demo->cache[slot].depth = 0;
        demo->cache[slot].label.clear();
    }

    void on_tree_pool_bind(void* ctx, int slot, int index, const TreeView::NodeInfo& info) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo) return;
        if (slot < 0 || slot >= static_cast<int>(demo->cache.size())) return;
        auto& entry = demo->cache[slot];
        entry.index = index;
        entry.depth = info.depth;
        entry.label = info.label ? info.label : "";
    }

    void on_tree_pool_recycle(void* ctx, int slot, int index) noexcept {
        (void)index;
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo) return;
        if (slot < 0 || slot >= static_cast<int>(demo->cache.size())) return;
        auto& entry = demo->cache[slot];
        entry.index = -1;
        entry.depth = 0;
        entry.label.clear();
    }

    struct TableDemoRow {
        const char* name{nullptr};
        int value{0};
        int delta{0};
    };

    struct TableDemo {
        static constexpr int kRows = 6;
        std::array<TableDemoRow, kRows> rows{};
        std::array<int, kRows> order{};
        bool sort_asc{true};
    };

    TableDemo g_table_demo{
        .rows = {{
            {"Buffer", 64, 2},
            {"Underrun", 0, 0},
            {"Latency", 120, -3},
            {"Seek", 4, 1},
            {"Decode", 7, 0},
            {"Render", 12, -1},
        }},
        .order = {},
        .sort_asc = true,
    };

    void table_rebuild_order(TableDemo& demo) noexcept {
        for (int i = 0; i < TableDemo::kRows; ++i) {
            demo.order[i] = demo.sort_asc ? i : (TableDemo::kRows - 1 - i);
        }
    }

    int table_row_count(void* ctx) noexcept {
        auto* demo = static_cast<TableDemo*>(ctx);
        return demo ? TableDemo::kRows : 0;
    }

    int table_col_count(void* ctx) noexcept {
        (void)ctx;
        return 3;
    }

    int table_col_width(void* ctx, int col) noexcept {
        (void)ctx;
        return (col == 0) ? 140 : 80;
    }

    void on_table_draw(void* ctx, DefaultCanvas& cvs, const TableView::CellInfo& info) noexcept {
        auto* demo = static_cast<TableDemo*>(ctx);
        if (!demo || info.row < 0 || info.row >= TableDemo::kRows) return;
        const int row = demo->order[info.row];
        const auto& item = demo->rows[row];
        char buf[32]{};
        const char* text = "";
        if (info.col == 0) {
            text = item.name ? item.name : "";
        } else if (info.col == 1) {
            std::snprintf(buf, sizeof(buf), "%d", item.value);
            text = buf;
        } else if (info.col == 2) {
            std::snprintf(buf, sizeof(buf), "%+d", item.delta);
            text = buf;
        }

        const auto& st = Theme::instance().get<TableView>();
        Rect text_box = info.rect;
        text_box.x += st.padding;
        text_box.w -= st.padding * 2;
        draw_text_box(cvs, text_box, text, st.font_color, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    }

    void on_table_select(void* ctx, int row, int col) noexcept {
        (void)ctx;
        (void)row;
        (void)col;
    }
#endif

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
        WidgetHandle play_mode{};
        WidgetHandle spectrum{};
        WidgetHandle ring{};
        WidgetHandle text_box{};
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
#endif
        WidgetHandle progress{};
        WidgetHandle time{};
        WidgetHandle btn_prev{};
        WidgetHandle btn_play{};
        WidgetHandle btn_pause{};
        WidgetHandle btn_next{};
        WidgetHandle btn_stop{};
        WidgetHandle controls{};
        WidgetHandle perf_overlay{};
    };

    struct PerfState {
        std::chrono::steady_clock::time_point last{};
        int frame_count{0};
        int fps{0};
        int draw_ms{0};
        int dirty_count{0};
        int dirty_area{0};
        int nodes{0};
        int depth_hits{0};
        int cycle_hits{0};
    };

    struct PlayerUiContext {
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
        bool duration_ready{false};
        bool ignore_list_select{false};
        std::string mount_status{};
        bool syncing_scrollbar{false};
        std::array<float, audio::AudioPlayer::spectrum_bins> spectrum_values{};
        std::array<int, audio::AudioPlayer::spectrum_bins> spectrum_points{};
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

        void set_status(const char* text) {
            set_label(handles.status, text);
        }

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

        void set_pause_button_text(const char* text) {
            if (!factory) return;
            if (auto* btn = factory->get_button(handles.btn_pause)) {
                btn->set_text(text);
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
            if (auto* mode = factory->get_segmented_control(handles.play_mode)) {
                mode->set_visible(!on);
            }
            if (auto* chart = factory->get_chart(handles.spectrum)) {
                chart->set_visible(!on);
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

        void update_progress() {
            if (!playing || updating) return;
            const auto now = std::chrono::steady_clock::now();
            const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - start).count());
            const int clamped = (elapsed > duration_sec) ? duration_sec : elapsed;
            const int value = (duration_sec > 0)
                ? static_cast<int>((clamped * 100) / duration_sec)
                : 0;

            if (auto* bar = factory->get_progress(handles.progress)) {
                bar->set_value(value);
            }
            set_time_label(clamped);
        }

        void update_spectrum() {
            if (!player || !factory) return;
            if (!player->read_spectrum(spectrum_values)) return;
            for (std::size_t i = 0; i < spectrum_values.size(); ++i) {
                const float v = spectrum_values[i];
                spectrum_points[i] = static_cast<int>(v * 100.0f);
            }
            if (auto* chart = factory->get_chart(handles.spectrum)) {
                chart->set_points(spectrum_points.data(),
                                  static_cast<int>(spectrum_points.size()));
            }
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
            set_pause_button_text("Pause");
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
            set_pause_button_text("Resume");
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
            set_pause_button_text("Pause");
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
            set_pause_button_text("Pause");
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
            set_pause_button_text("Pause");
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

    static PlayerUiContext g_ctx{};

    void on_play_clicked(void* ctx) {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->start_playback();
    }

    void on_pause_clicked(void* ctx) {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        if (app->playing) app->pause_playback();
        else if (app->paused) app->resume_playback();
    }

    void on_stop_clicked(void* ctx) {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->stop_playback();
    }

    void on_prev_clicked(void* ctx) {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->switch_track(-1);
    }

    void on_next_clicked(void* ctx) {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->switch_track(1);
    }

    void on_list_selected(void* ctx, int index) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app || app->ignore_list_select) return;
        app->select_track_index(index);
    }

    void on_list_pool_create(void* ctx, int slot) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        if (slot < 0 || slot >= static_cast<int>(app->list_cache.size())) return;
        auto& entry = app->list_cache[slot];
        entry.index = -1;
        entry.width = 0;
        entry.text.clear();
    }

    void on_list_pool_bind(void* ctx, int slot, int index) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        if (slot < 0 || slot >= static_cast<int>(app->list_cache.size())) return;
        if (index < 0 || index >= static_cast<int>(app->track_labels.size())) return;
        auto& entry = app->list_cache[slot];
        entry.index = index;
        entry.width = 0;
        entry.text = app->track_labels[index];
    }

    void on_list_pool_recycle(void* ctx, int slot, int index) noexcept {
        (void)index;
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        if (slot < 0 || slot >= static_cast<int>(app->list_cache.size())) return;
        auto& entry = app->list_cache[slot];
        entry.index = -1;
        entry.width = 0;
        entry.text.clear();
    }

    void on_list_draw(void* ctx, DefaultCanvas& cvs, const ListView::DrawInfo& info) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        const Style& st = Theme::instance().get<ListView>();
        const auto font = resolve_font(st);
        Rect text = info.rect;
        text.x += st.padding;
        text.w -= st.padding * 2;
        if (text.w <= 0 || text.h <= 0) return;
        const char* label = nullptr;
        if (info.slot >= 0 && info.slot < static_cast<int>(app->list_cache.size())) {
            const auto& entry = app->list_cache[info.slot];
            if (entry.index == info.index && !entry.text.empty()) {
                label = entry.text.c_str();
            }
        }
        if (!label) {
            if (info.index < 0 || info.index >= static_cast<int>(app->track_labels.size())) return;
            label = app->track_labels[info.index].c_str();
        }
        const rgba color = st.font_color;
        draw_text_box(cvs, text, label, color, font,
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    }

    void on_list_scrolled(void* ctx, int scroll_y, int max_scroll, int view_h, int content_h) noexcept {
        (void)content_h;
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app || !app->factory) return;
        auto* bar = app->factory->get_scroll_bar(app->handles.list_scroll);
        if (!bar) return;
        app->syncing_scrollbar = true;
        bar->set_range(0, max_scroll);
        bar->set_page_size(view_h);
        bar->set_value(scroll_y);
        app->syncing_scrollbar = false;
    }

    void on_list_scrollbar_change(void* ctx) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app || !app->factory || app->syncing_scrollbar) return;
        auto* bar = app->factory->get_scroll_bar(app->handles.list_scroll);
        auto* list = app->factory->get_list_view(app->handles.list);
        if (!bar || !list) return;
        list->set_scroll_y(bar->value());
    }

    void on_play_mode_change(void* ctx) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app || !app->factory) return;
        auto* mode = app->factory->get_segmented_control(app->handles.play_mode);
        if (!mode) return;
        app->play_mode = mode->selected();
    }

    Event::Key map_key(SDL_Keycode key) {
        switch (key) {
        case SDLK_TAB: return Event::Key::Tab;
        case SDLK_RETURN: return Event::Key::Enter;
        case SDLK_SPACE: return Event::Key::Space;
        case SDLK_ESCAPE: return Event::Key::Escape;
        case SDLK_UP: return Event::Key::Up;
        case SDLK_DOWN: return Event::Key::Down;
        case SDLK_LEFT: return Event::Key::Left;
        case SDLK_RIGHT: return Event::Key::Right;
        default: return Event::Key::Unknown;
        }
    }

    void apply_cover_pulse(void* ctx, float v) {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        const std::uint8_t base = 40;
        const std::uint8_t delta = static_cast<std::uint8_t>(20 * v);
        app->set_cover_color({static_cast<std::uint8_t>(base + delta),
                              static_cast<std::uint8_t>(44 + delta),
                              static_cast<std::uint8_t>(60 + delta),
                              255});
    }

    bool dispatch_sdl_event(Gui& gui, PlayerUiContext& ctx, const SDL_Event& evt) {
        switch (evt.type) {
        case SDL_EVENT_MOUSE_MOTION:
            if (ctx.dragging) {
                ctx.update_drag(evt.motion.x);
                return true;
            }
            gui.dispatch_event(Event::mouse(Event::Type::MouseMove, evt.motion.x, evt.motion.y));
            return true;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (evt.button.button == SDL_BUTTON_LEFT) {
                if (ctx.begin_drag(evt.button.x, evt.button.y)) {
                    return true;
                }
            }
            gui.dispatch_event(Event::mouse(Event::Type::MouseDown, evt.button.x, evt.button.y, evt.button.button));
            return true;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (evt.button.button == SDL_BUTTON_LEFT && ctx.dragging) {
                ctx.end_drag();
                return true;
            }
            gui.dispatch_event(Event::mouse(Event::Type::MouseUp, evt.button.x, evt.button.y, evt.button.button));
            return true;
        case SDL_EVENT_MOUSE_WHEEL:
            gui.dispatch_event(Event::wheel(evt.wheel.x, evt.wheel.y, evt.wheel.y));
            return true;
        case SDL_EVENT_KEY_DOWN:
#if CHARM_PLAYER_DEBUG_UI
            if (evt.key.key == SDLK_T) {
                ctx.toggle_debug_view();
                return true;
            }
            if (evt.key.key == SDLK_S) {
                g_table_demo.sort_asc = !g_table_demo.sort_asc;
                table_rebuild_order(g_table_demo);
                return true;
            }
            if (evt.key.key == SDLK_G) {
                const auto e = Event::gesture(Event::Type::GestureSwipe,
                                              0, 0,
                                              0, 120,
                                              Event::GesturePhase::Update,
                                              1.0f);
                gui.dispatch_event(e);
                return true;
            }
            if (ctx.show_debug) {
                Event::Key key = Event::Key::Unknown;
                if (evt.key.key == SDLK_UP) key = Event::Key::Up;
                else if (evt.key.key == SDLK_DOWN) key = Event::Key::Down;
                else if (evt.key.key == SDLK_LEFT) key = Event::Key::Left;
                else if (evt.key.key == SDLK_RIGHT) key = Event::Key::Right;
                else if (evt.key.key == SDLK_RETURN) key = Event::Key::Enter;
                else if (evt.key.key == SDLK_SPACE) key = Event::Key::Space;
                if (key != Event::Key::Unknown) {
                    if (ctx.menu_tree.handle_event(Event::key(Event::Type::KeyDown, key))) {
                        return true;
                    }
                }
            }
#endif
            if (evt.key.key == SDLK_SPACE) {
                if (ctx.playing) ctx.pause_playback();
                else if (ctx.paused) ctx.resume_playback();
                else ctx.start_playback();
                return true;
            }
            if (evt.key.key == SDLK_N) {
                ctx.switch_track(1);
                return true;
            }
            if (evt.key.key == SDLK_P) {
                ctx.switch_track(-1);
                return true;
            }
            gui.dispatch_event(Event::key(Event::Type::KeyDown, map_key(evt.key.key)));
            return true;
        case SDL_EVENT_KEY_UP:
            gui.dispatch_event(Event::key(Event::Type::KeyUp, map_key(evt.key.key)));
            return true;
        default:
            return false;
        }
    }

    UiHandles build_ui(UiFactory& factory, PlayerUiContext& ctx) {
        auto anchor_pos = [](auto* obj, int x, int y) {
            if (!obj) return;
            obj->set_pos(x, y);
            obj->set_anchor(x, y, -1, -1);
        };
        auto anchor_rect = [](auto* obj, const Rect& r) {
            if (!obj) return;
            obj->set_rect(r);
            obj->set_anchor(r.x, r.y, -1, -1);
        };
        UiHandles h{};
        h.root = factory.create_container();
        auto* root = factory.get_container(h.root);
        root->set_rect({0, 0, screen_width, screen_height});
        root->set_background(kUiBackground);

        const int cover_top = kUiPadding * 2;
        const int header_top = cover_top + kCoverSize;
        const int mode_y = header_top + kHeaderModeOffset;
        const int spectrum_y = mode_y + kModeHeight + kSpectrumGap;
        const int list_y = spectrum_y + kSpectrumHeight + kSpectrumGap;
        const int list_h = screen_height - list_y - kListBottomReserve;

        h.cover = factory.create_container();
        if (auto* cover = factory.get_container(h.cover)) {
            anchor_rect(cover, {(screen_width - kCoverSize) / 2, cover_top, kCoverSize, kCoverSize});
            cover->set_background(kUiCover);
        }

        h.title = factory.create_label("Beautiful Trick");
        if (auto* title = factory.get_label(h.title)) {
            title->set_color(kUiTitle);
            anchor_rect(title, {kUiPadding, header_top + kHeaderTitleOffset,
                                screen_width - kUiPadding * 2, 24});
            title->set_align(TextAlignH::Center, TextAlignV::Center);
        }

        h.subtitle = factory.create_label("FELT · FLAC");
        if (auto* sub = factory.get_label(h.subtitle)) {
            sub->set_color(kUiSubtitle);
            anchor_rect(sub, {kUiPadding, header_top + kHeaderSubtitleOffset,
                              screen_width - kUiPadding * 2, 20});
            sub->set_align(TextAlignH::Center, TextAlignV::Center);
        }

        h.progress = factory.create_progress();
        if (auto* bar = factory.get_progress(h.progress)) {
            anchor_rect(bar, {kUiPadding, header_top + kHeaderProgressOffset,
                              screen_width - kUiPadding * 2, 16});
            bar->set_range(0, 100);
            bar->set_value(0);
        }

        h.time = factory.create_label("0:00 / 3:00");
        if (auto* time = factory.get_label(h.time)) {
            time->set_color(kUiTime);
            anchor_rect(time, {kUiPadding, header_top + kHeaderTimeOffset,
                               screen_width - kUiPadding * 2, 18});
            time->set_align(TextAlignH::Center, TextAlignV::Center);
        }

        h.status = factory.create_label("Stopped");
        if (auto* status = factory.get_label(h.status)) {
            status->set_color(kUiStatus);
            anchor_rect(status, {kUiPadding, header_top + kHeaderStatusOffset,
                                 screen_width - kUiPadding * 2, 18});
            status->set_align(TextAlignH::Center, TextAlignV::Center);
        }

        h.play_mode = factory.create_segmented_control();
        if (auto* mode = factory.get_segmented_control(h.play_mode)) {
            static const char* kModeItems[] = {"Order", "Single", "Shuffle"};
            mode->set_items(kModeItems, 3);
            mode->set_selected(0);
            mode->set_on_change(Callback{&on_play_mode_change, &ctx});
            anchor_rect(mode, {(screen_width - kModeWidth) / 2, mode_y, kModeWidth, kModeHeight});
        }

        h.spectrum = factory.create_chart();
        if (auto* chart = factory.get_chart(h.spectrum)) {
            anchor_rect(chart, {kUiPadding, spectrum_y, screen_width - kUiPadding * 2, kSpectrumHeight});
        }

        h.perf_overlay = factory.create_perf_overlay();
        if (auto* perf = factory.get_perf_overlay(h.perf_overlay)) {
            anchor_rect(perf, {screen_width - (kPerfOverlayWidth + kUiPadding),
                               kUiPadding, kPerfOverlayWidth, kPerfOverlayHeight});
        }

        h.ring = factory.create_ring_indication();
        if (auto* ring = factory.get_ring_indication(h.ring)) {
            anchor_rect(ring, {screen_width - kUiPadding - 90, kUiPadding + 90, 90, 90});
            ring->set_value(58);
            ring->set_thickness(10);
        }

        h.text_box = factory.create_text_box("ARM-2D style text box\nwrap + padding");
        if (auto* box = factory.get_text_box(h.text_box)) {
            anchor_rect(box, {screen_width - kUiPadding - 200, kUiPadding + 190, 200, 70});
        }

#if CHARM_PLAYER_DEBUG_UI
        h.debug_grid = factory.create_container();
        if (auto* grid = factory.get_container(h.debug_grid)) {
            const int half_w = (screen_width - kUiPadding * 2 - kDemoGap) / 2;
            const int cell_h = (list_h - kDemoGap) / 2;
            anchor_rect(grid, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});
            grid->set_grid_layout(2, half_w, cell_h, kDemoGap, 0);
            grid->set_align(static_cast<int>(AlignH::Center), static_cast<int>(AlignV::Center));
            grid->set_visible(false);
        }
#endif

        h.list_title = factory.create_label("Tracks");
        if (auto* title = factory.get_label(h.list_title)) {
            title->set_color(kUiListTitle);
            anchor_rect(title, {kUiPadding, list_y - kListTitleGap, screen_width - kUiPadding * 2, 18});
            title->set_align(TextAlignH::Left, TextAlignV::Center);
        }

        h.list = factory.create_list_view();
        if (auto* list = factory.get_list_view(h.list)) {
            anchor_rect(list, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});
            list->set_on_draw(&on_list_draw, &ctx);
            list->set_on_select(&on_list_selected, &ctx);
            list->set_item_pool(&on_list_pool_create, &on_list_pool_bind, &on_list_pool_recycle, &ctx);
            list->set_prefetch_rows(2);
            list->set_row_height(32);
            list->set_wheel_step(32);
        }

        h.list_scroll = factory.create_scroll_bar();
        if (auto* bar = factory.get_scroll_bar(h.list_scroll)) {
            anchor_rect(bar, {screen_width - kUiPadding - kListScrollWidth, list_y, kListScrollWidth, list_h});
            bar->set_orientation(ScrollBar::Orientation::Vertical);
            bar->set_on_change(Callback{&on_list_scrollbar_change, &ctx});
        }

        h.list_hint = factory.create_label("No tracks in /music or /");
        if (auto* hint = factory.get_label(h.list_hint)) {
            hint->set_color(kUiHint);
            anchor_rect(hint, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});
            hint->set_align(TextAlignH::Center, TextAlignV::Center);
            hint->set_visible(false);
        }
 
#if CHARM_PLAYER_DEBUG_UI
        h.tree = factory.create_tree_view();
        if (auto* tree = factory.get_tree_view(h.tree)) {
            tree_rebuild_visible(g_tree_demo);
            tree->set_data_source(&tree_row_count, &tree_node_info, &on_tree_draw, &g_tree_demo);
            tree->set_on_toggle(&on_tree_toggle);
            tree->set_item_pool(&on_tree_pool_create, &on_tree_pool_bind, &on_tree_pool_recycle, &g_tree_demo);
            tree->set_prefetch_rows(2);
            tree->set_row_height(28);
        }

        h.table = factory.create_table_view();
        if (auto* table = factory.get_table_view(h.table)) {
            table_rebuild_order(g_table_demo);
            table->set_data_source(&table_row_count, &table_col_count, &on_table_draw, &g_table_demo);
            table->set_column_width_fn(&table_col_width);
            table->set_on_select(&on_table_select, &g_table_demo);
            table->set_row_height(28);
        }

        h.chart = factory.create_chart();
        if (auto* chart = factory.get_chart(h.chart)) {
            static int points[] = {3, 8, 5, 12, 6, 14, 7, 10, 4};
            chart->set_points(points, static_cast<int>(sizeof(points) / sizeof(points[0])));
        }

        h.debug_side = factory.create_container();
        if (auto* side = factory.get_container(h.debug_side)) {
            side->set_flex_layout(1, 0, 0, 8, 8);
            side->set_flex_grow(1);
        }

        ctx.menu_tree.init(factory, h.debug_side);
        ctx.menu_tree.set_rect({0, 0, 200, 120});
        ctx.menu_tree.set_item_height(22);
        ctx.menu_tree.set_indent(12);
        const int menu_file = ctx.menu_tree.add_item(-1, "File");
        ctx.menu_tree.add_item(menu_file, "New");
        ctx.menu_tree.add_item(menu_file, "Open");
        ctx.menu_tree.add_item(menu_file, "Save");
        const int menu_edit = ctx.menu_tree.add_item(-1, "Edit");
        ctx.menu_tree.add_item(menu_edit, "Undo");
        ctx.menu_tree.add_item(menu_edit, "Redo");
        ctx.menu_tree.add_item(-1, "View");
        ctx.menu_tree.set_expanded(menu_file, true);
        ctx.menu_tree.rebuild();

        h.logo = factory.create_image();
        if (auto* logo = factory.get_image(h.logo)) {
            logo->set_image(render_logo_argb());
            logo->set_scale_mode(Image::ScaleMode::Fit);
            logo->set_alignment(Image::AlignH::Center, Image::AlignV::Center);
            logo->set_rotation(Image::Rotation::Rotate90);
            logo->set_sampling(Image::Sampling::Bilinear);
            logo->set_crop_mode(Image::CropMode::Transparent);
            logo->set_edge_mode(Image::EdgeMode::AllowOutside);
            logo->set_anchor(0.5f, 0.5f);
            logo->set_crop({4, 4, 22, 22});
            logo->set_size(200, 90);
        }

        h.stepper = factory.create_stepper();
        if (auto* stepper = factory.get_stepper(h.stepper)) {
            stepper->set_steps(4);
            stepper->set_current(1);
            stepper->set_label(0, "Init");
            stepper->set_label(1, "Load");
            stepper->set_label(2, "Play");
            stepper->set_label(3, "Done");
            stepper->set_size(200, 48);
        }

        h.timeline = factory.create_timeline();
        if (auto* timeline = factory.get_timeline(h.timeline)) {
            timeline->set_item_count(4);
            timeline->set_item_text(0, "Boot");
            timeline->set_item_text(1, "Scan");
            timeline->set_item_text(2, "Decode");
            timeline->set_item_text(3, "Ready");
            timeline->set_current(2);
            timeline->set_row_height(26);
            timeline->set_size(200, 140);
            timeline->set_flex_grow(1);
        }

        h.rich_text = factory.create_rich_text();
        if (auto* rich = factory.get_rich_text(h.rich_text)) {
            rich->set_text("[b]Rich[/b] [color=#7ED321]text[/color] demo\n"
                           "[mono]mono[/mono] and [code]code[/code] sample");
            rich->set_size(200, 70);
        }

        h.code_block = factory.create_code_block();
        if (auto* block = factory.get_code_block(h.code_block)) {
            block->set_text("int main() {\n  return 0;\n}");
            block->set_wrap(TextWrap::None);
            block->set_size(200, 80);
        }

        h.progress_wheel = factory.create_progress_wheel();
        if (auto* wheel = factory.get_progress_wheel(h.progress_wheel)) {
            wheel->set_value(72);
            wheel->set_thickness(8);
            wheel->set_size(90, 90);
        }

        h.waveform = factory.create_waveform_view();
        if (auto* wave = factory.get_waveform_view(h.waveform)) {
            static int samples[] = {3, 5, 8, 6, 2, -2, -5, -3, 1, 4, 7, 4, 1, -3, -6, -4};
            wave->set_samples(samples, static_cast<int>(sizeof(samples) / sizeof(samples[0])));
            wave->set_size(200, 80);
        }

        h.battery_gauge = factory.create_battery_gauge();
        if (auto* battery = factory.get_battery_gauge(h.battery_gauge)) {
            battery->set_value(65);
            battery->set_size(200, 48);
        }

        h.histogram = factory.create_histogram_view();
        if (auto* hist = factory.get_histogram_view(h.histogram)) {
            static int bins[] = {2, 5, 8, 3, 6, 9, 4, 7, 5, 2, 6, 8};
            hist->set_values(bins, static_cast<int>(sizeof(bins) / sizeof(bins[0])));
            hist->set_size(200, 80);
        }

        h.fold_panel = factory.create_foldable_panel("Foldable Panel");
        if (auto* panel = factory.get_foldable_panel(h.fold_panel)) {
            panel->set_body("Tap header to expand or collapse.");
            panel->set_expanded(true);
            panel->set_size(200, 120);
        }
        h.progress_flow = factory.create_progress_flowing();
        if (auto* flow = factory.get_progress_flowing(h.progress_flow)) {
            flow->set_range(0, 100);
            flow->set_indeterminate(true);
            flow->set_flow_span(12);
            flow->set_flow_speed(2);
            flow->set_size(200, 16);
        }
        h.cloudy_glass = factory.create_cloudy_glass();
        if (auto* glass = factory.get_cloudy_glass(h.cloudy_glass)) {
            glass->set_size(200, 70);
            glass->set_opacity(140);
        }
        auto fold_btn_primary = factory.create_button("Apply");
        if (auto* btn = factory.get_button(fold_btn_primary)) {
            btn->set_size(90, 32);
        }
        auto fold_btn_secondary = factory.create_button("Reset");
        if (auto* btn = factory.get_button(fold_btn_secondary)) {
            btn->set_size(90, 32);
        }
        factory.link(h.fold_panel, fold_btn_primary);
        factory.link(h.fold_panel, fold_btn_secondary);
#endif

        const int controls_w = kButtonWidth * 3 + kButtonGap * 2;
        const int controls_h = kButtonHeight * 2 + kButtonGap;
        const int controls_x = (screen_width - controls_w) / 2;
        const int controls_y = screen_height - controls_h - kControlsBottomMargin;
        h.controls = factory.create_container();
        if (auto* controls = factory.get_container(h.controls)) {
            anchor_rect(controls, {controls_x, controls_y, controls_w, controls_h});
            controls->set_flow_layout(kButtonGap, kButtonGap, 0);
            controls->set_align(static_cast<int>(AlignH::Center), static_cast<int>(AlignV::Start));
        }

        h.btn_prev = factory.create_button("Prev");
        if (auto* prev = factory.get_button(h.btn_prev)) {
            prev->set_size(kButtonWidth, kButtonHeight);
            prev->set_on_click(Callback{&on_prev_clicked, &ctx});
        }

        h.btn_next = factory.create_button("Next");
        if (auto* next = factory.get_button(h.btn_next)) {
            next->set_size(kButtonWidth, kButtonHeight);
            next->set_on_click(Callback{&on_next_clicked, &ctx});
        }

        h.btn_play = factory.create_button("Play");
        if (auto* play = factory.get_button(h.btn_play)) {
            play->set_size(kButtonWidth, kButtonHeight);
            play->set_on_click(Callback{&on_play_clicked, &ctx});
        }

        h.btn_pause = factory.create_button("Pause");
        if (auto* pause = factory.get_button(h.btn_pause)) {
            pause->set_size(kButtonWidth, kButtonHeight);
            pause->set_on_click(Callback{&on_pause_clicked, &ctx});
        }

        h.btn_stop = factory.create_button("Stop");
        if (auto* stop = factory.get_button(h.btn_stop)) {
            stop->set_size(kButtonWidth, kButtonHeight);
            stop->set_on_click(Callback{&on_stop_clicked, &ctx});
        }

        factory.link(h.root, h.cover);
        factory.link(h.root, h.title);
        factory.link(h.root, h.subtitle);
        factory.link(h.root, h.progress);
        factory.link(h.root, h.time);
        factory.link(h.root, h.status);
        factory.link(h.root, h.play_mode);
        factory.link(h.root, h.spectrum);
        factory.link(h.root, h.list_title);
        factory.link(h.root, h.list);
        factory.link(h.root, h.list_scroll);
        factory.link(h.root, h.list_hint);
        factory.link(h.root, h.ring);
        factory.link(h.root, h.text_box);
#if CHARM_PLAYER_DEBUG_UI
        factory.link(h.root, h.debug_grid);
        factory.link(h.debug_grid, h.tree);
        factory.link(h.debug_grid, h.table);
        factory.link(h.debug_grid, h.chart);
        factory.link(h.debug_grid, h.debug_side);
        factory.link(h.debug_side, h.logo);
        factory.link(h.debug_side, h.stepper);
        factory.link(h.debug_side, h.timeline);
        factory.link(h.debug_side, h.rich_text);
        factory.link(h.debug_side, h.code_block);
        factory.link(h.debug_side, h.progress_wheel);
        factory.link(h.debug_side, h.waveform);
        factory.link(h.debug_side, h.battery_gauge);
        factory.link(h.debug_side, h.histogram);
        factory.link(h.debug_side, h.fold_panel);
        factory.link(h.debug_side, h.progress_flow);
        factory.link(h.debug_side, h.cloudy_glass);
#endif
        factory.link(h.root, h.controls);
        factory.link(h.controls, h.btn_prev);
        factory.link(h.controls, h.btn_play);
        factory.link(h.controls, h.btn_pause);
        factory.link(h.controls, h.btn_stop);
        factory.link(h.controls, h.btn_next);
        factory.bring_to_front(h.root, h.perf_overlay);

        return h;
    }

}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    const char* vhd_path = kDefaultVhdPath;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Charm Player", screen_width, screen_height, 0);
    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                             screen_width, screen_height);
    if (!texture) {
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    g_ctx.player = &g_player;
    g_ctx.factory = &g_factory;
    g_ctx.track_path = nullptr;
    g_ctx.tracks = &g_vfs_tracks;
    g_player.enable_spectrum(true);

    apply_player_theme();

#if CHARM_PLAYER_DEBUG_UI
    auto& theme = Theme::instance();
    theme.inherit<TableView, ListView>();
    theme.inherit<TreeView, ListView>();
    StylePatch table_patch{};
    table_patch.has_bg_color = true;
    table_patch.bg_color = kUiListBg;
    table_patch.has_border_color = true;
    table_patch.border_color = kUiListBorder;
    table_patch.has_padding = true;
    table_patch.padding = 6;
    theme.patch<TableView>(table_patch);
    StylePatch tree_patch = table_patch;
    tree_patch.bg_color = {20, 22, 30, 255};
    theme.patch<TreeView>(tree_patch);
#endif

    g_ctx.handles = build_ui(g_factory, g_ctx);
    g_ctx.set_time_label(0);
    g_ctx.mount_status = "Mounting VHD...";
    g_ctx.set_status("Mounting");
    g_ctx.set_status_color(kUiStatus);
    g_ctx.update_list_placeholder();

    const auto mount_st = mount_fatfs_from_vhd(vhd_path);
    g_ctx.fs_ready = static_cast<bool>(mount_st);
    if (!g_ctx.fs_ready) {
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "Mount failed (%s)", fs_err_text(mount_st.err));
        g_ctx.set_status(buf);
        g_ctx.set_status_color(kUiError);
        g_ctx.mount_status = std::string(buf) + ". Unmount VHD in Windows";
        g_vfs_tracks.clear();
        g_ctx.rebuild_track_labels();
        g_ctx.refresh_list_view();
        g_ctx.track_ready = false;
        g_ctx.track_path = nullptr;
        g_ctx.set_pause_button_text("Pause");
        g_ctx.set_time_label(0);
        g_ctx.sync_progress_value(0);
        g_ctx.reset_duration();
        g_ctx.update_list_placeholder();
    } else {
        g_ctx.mount_status = "Mounted";
        g_ctx.update_list_placeholder();
        g_vfs_tracks.clear();
        fs::Status list_st{fs::Err::ok};
        g_ctx.mount_status = "Scanning /music...";
        if (!collect_tracks_from_dir("/music", g_vfs_tracks, nullptr, list_st)) {
            std::vector<std::string> subdirs;
            g_ctx.mount_status = "Scanning /...";
            const bool root_has = collect_tracks_from_dir("/", g_vfs_tracks, &subdirs, list_st);
            if (list_st) {
                for (const auto& dir : subdirs) {
                    collect_tracks_from_dir(dir, g_vfs_tracks, nullptr, list_st);
                }
            }
            if (!list_st) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "List failed (%s)", fs_err_text(list_st.err));
                g_ctx.set_status(buf);
                g_ctx.set_status_color(kUiError);
                g_ctx.mount_status = buf;
            } else if (!root_has && g_vfs_tracks.empty()) {
                g_ctx.set_status("No tracks found");
                g_ctx.set_status_color(kUiError);
                g_ctx.mount_status = "No tracks in /music or /";
            }
        }
        bool should_load = true;
        if (g_vfs_tracks.empty()) {
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), "No tracks (%s)", fs_err_text(list_st.err));
            g_ctx.set_status(buf);
            g_ctx.set_status_color(kUiError);
            if (g_ctx.mount_status == "Mounted") {
                g_ctx.mount_status = "No tracks in /music or /";
            }
            should_load = false;
        }
        g_ctx.rebuild_track_labels();
        g_ctx.refresh_list_view();
        if (should_load) {
            g_ctx.load_track_index(0);
            if (g_ctx.track_ready && !fs_seek_selftest(g_ctx.track_path)) {
                g_ctx.set_status("Fs seek selftest failed");
                g_ctx.set_status_color(kUiError);
            }
            if (g_ctx.track_ready) {
                g_ctx.mount_status = "Ready";
            }
        } else {
            g_ctx.track_ready = false;
            g_ctx.track_path = nullptr;
            g_ctx.set_pause_button_text("Pause");
            g_ctx.set_time_label(0);
            g_ctx.sync_progress_value(0);
            g_ctx.reset_duration();
        }
        g_ctx.update_list_placeholder();
    }

    auto gui = std::make_unique<Gui>(g_canvas, g_factory, g_ctx.handles.root);
    gui->set_dirty_tracking(true);
    gui->set_layer_cache(true);

    const auto start_time = std::chrono::steady_clock::now();

    bool running = true;
    while (running) {
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            dispatch_sdl_event(*gui, g_ctx, evt);
        }

        g_player.tick();
        if (g_ctx.playing && !g_player.is_running()) {
            if (g_player.state() == audio::PlayerState::error) {
                g_ctx.playing = false;
                g_ctx.paused = false;
                g_ctx.set_status("Stopped");
                g_ctx.set_status_color(kUiStatus);
                g_ctx.set_pause_button_text("Pause");
            } else {
                g_ctx.handle_track_end();
            }
        }
        if (!g_ctx.paused) {
            const auto st = g_player.state();
            if (st == audio::PlayerState::opening) {
                g_ctx.set_status("Opening");
                g_ctx.set_status_color(kUiStatus);
            } else if (st == audio::PlayerState::buffering) {
                g_ctx.set_status("Buffering");
                g_ctx.set_status_color(kUiStatus);
            } else if (st == audio::PlayerState::playing) {
                g_ctx.set_status("Playing");
                g_ctx.set_status_color(kUiOk);
            }
        }
        if (g_player.state() == audio::PlayerState::error) {
            g_ctx.playing = false;
            g_ctx.paused = false;
            const auto err = g_player.last_error();
            const auto stage = static_cast<audio::PlayerErrorStage>(err.ext);
            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "Player error (%s/%s)",
                          audio_err_text(err.code), audio_stage_text(stage));
            g_ctx.set_status(buf);
            g_ctx.set_status_color(kUiError);
            g_ctx.set_pause_button_text("Pause");
        }
        g_ctx.update_duration_from_player();
        g_ctx.apply_pending_seek();
        g_ctx.update_progress();
        g_ctx.update_spectrum();

        const auto now = std::chrono::steady_clock::now();
        const auto ms = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count());
        const float phase = static_cast<float>(ms % 2000) / 2000.0f;
        const float v = 0.5f - 0.5f * std::cos(phase * 6.2831853f);
        apply_cover_pulse(&g_ctx, v);

        g_canvas.clear(kUiBackground);
        gui->render();

        SDL_UpdateTexture(texture, nullptr, g_framebuffer.data(), screen_width * 3);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    (void)g_player.stop();
    for (int i = 0; i < 8 && g_player.is_running(); ++i) {
        g_player.tick();
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

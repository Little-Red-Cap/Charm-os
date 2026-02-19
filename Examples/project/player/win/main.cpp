import audio.player;
import audio.result;
import audio.source.fs;
import charm.core.config;
import charm.core.container;
import charm.core.event;
import charm.core.factory;
import charm.core.gui;
import charm.core.layout;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.gfx.assets.render;
import charm.gfx.framebuffer;
import charm.gfx.color;
import charm.widgets.button;
import charm.widgets.label;
import charm.widgets.list_view;
import charm.widgets.progress;
import charm.widgets.scrollbar;
import charm.widgets.perf_overlay;
import charm.widgets.chart;
import charm.widgets.stepper;
import charm.widgets.timeline;
import charm.widgets.menu_tree;
import charm.widgets.rich_text;
import charm.widgets.code_block;
import charm.widgets.table_view;
import charm.widgets.text;
import fs_core;
import fs_errno;
import fs_block;
import fs_stream;
import fs_block_file;
import fs_fatfs;
import fs_vfs;
import util.core;

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {
    // constexpr const char* kDefaultVfsTrack = "/music/beautiful-trick.flac";
    constexpr const char* kDefaultVfsTrack = "/Beautiful Trick-FELT.flac";
    constexpr const char* kDefaultVhdPath = "G:/Project/dev.vhd";
    constexpr int kUiPadding = 24;
    constexpr int kCoverSize = 320;
    struct MbrPartition {
        std::uint8_t status;
        std::uint8_t chs_first[3];
        std::uint8_t type;
        std::uint8_t chs_last[3];
        std::uint32_t lba_first;
        std::uint32_t sectors;
    };

    std::uint32_t find_fat_partition_lba(const std::array<std::uint8_t, 512>& sector0) {
        if (sector0[510] != 0x55 || sector0[511] != 0xAA) return 0;
        const auto* parts = reinterpret_cast<const MbrPartition*>(sector0.data() + 446);
        for (int i = 0; i < 4; ++i) {
            const auto& p = parts[i];
            if (p.type == 0x0B || p.type == 0x0C) {
                return p.lba_first;
            }
        }
        return 0;
    }

    const char* fs_err_text(fs::Err err) {
        switch (err) {
        case fs::Err::ok: return "ok";
        case fs::Err::perm: return "perm";
        case fs::Err::noent: return "noent";
        case fs::Err::exist: return "exist";
        case fs::Err::io: return "io";
        case fs::Err::busy: return "busy";
        case fs::Err::inval: return "inval";
        case fs::Err::nametoolong: return "nametoolong";
        case fs::Err::nosys: return "nosys";
        case fs::Err::nomem: return "nomem";
        case fs::Err::notsup: return "notsup";
        case fs::Err::rofs: return "rofs";
        case fs::Err::timeout: return "timeout";
        case fs::Err::again: return "again";
        }
        return "unknown";
    }

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

    bool has_audio_ext(std::string_view name) {
        const auto dot = name.find_last_of('.');
        if (dot == std::string_view::npos || dot + 1 >= name.size()) return false;
        const auto ext = name.substr(dot + 1);
        if (ext.size() == 3) {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
            if (a == 'm' && b == 'p' && c == '3') return true;
            if (a == 'w' && b == 'a' && c == 'v') return true;
            if (a == 'f' && b == 'l' && c == 'a') return true;
        }
        if (ext.size() == 4) {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
            const char d = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[3])));
            if (a == 'f' && b == 'l' && c == 'a' && d == 'c') return true;
        }
        return false;
    }

    struct TrackListContext {
        std::string_view dir{};
        std::vector<std::string>* out{nullptr};
        std::vector<std::string>* subdirs{nullptr};
    };

    fs::Status collect_track(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
        auto* info = static_cast<TrackListContext*>(ctx);
        if (!info || !info->out) return fs::Status{fs::Err::inval};
        if (entry.type == fs::NodeType::dir) {
            if (!info->subdirs) return fs::Status{fs::Err::ok};
            std::string path;
            if (info->dir.empty() || info->dir == "/") {
                path = "/";
            } else {
                path.assign(info->dir.begin(), info->dir.end());
                if (!path.empty() && path.back() != '/') path.push_back('/');
            }
            path.append(entry.name.begin(), entry.name.end());
            info->subdirs->push_back(path);
            return fs::Status{fs::Err::ok};
        }
        if (entry.type != fs::NodeType::file) return fs::Status{fs::Err::ok};
        if (!has_audio_ext(entry.name)) return fs::Status{fs::Err::ok};

        std::string path;
        if (info->dir.empty() || info->dir == "/") {
            path = "/";
        } else {
            path.assign(info->dir.begin(), info->dir.end());
            if (!path.empty() && path.back() != '/') path.push_back('/');
        }
        path.append(entry.name.begin(), entry.name.end());
        info->out->push_back(path);
        return fs::Status{fs::Err::ok};
    }

    bool collect_tracks_from_dir(std::string_view dir,
                                 std::vector<std::string>& out,
                                 std::vector<std::string>* subdirs,
                                 fs::Status& out_status) {
        TrackListContext ctx{dir, &out, subdirs};
        out_status = fs::vfs_list(dir, &ctx, &collect_track);
        return out_status && !out.empty();
    }

    struct OffsetDevice {
        fs::BlockDevice* base{nullptr};
        std::uint32_t lba_offset{0};
        fs::BlockDevice device{};

        static fs::Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> out) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->read(self->base->ctx, lba + self->lba_offset, out);
        }
        static fs::Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> in) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->write(self->base->ctx, lba + self->lba_offset, in);
        }
        static fs::Status erase_impl(void* ctx, util::u64 lba, util::u64 count) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->erase(self->base->ctx, lba + self->lba_offset, count);
        }
        static fs::Status flush_impl(void* ctx) noexcept {
            auto* self = static_cast<OffsetDevice*>(ctx);
            return self->base->flush ? self->base->flush(self->base->ctx) : fs::Status{fs::Err::ok};
        }

        void init(fs::BlockDevice& dev, std::uint32_t offset) noexcept {
            base = &dev;
            lba_offset = offset;
            device.ctx = this;
            device.read = &OffsetDevice::read_impl;
            device.write = &OffsetDevice::write_impl;
            device.erase = &OffsetDevice::erase_impl;
            device.flush = &OffsetDevice::flush_impl;
            device.block_size = dev.block_size;
            device.block_count = dev.block_count > offset ? (dev.block_count - offset) : 0;
        }
    };

    fs::Status mount_fatfs_from_vhd(const char* path) {
        if (!path || !*path) return fs::Status{fs::Err::inval};
        static fs::BlockFile file_dev;
        static OffsetDevice part_dev;
        static fs::FatFsMount fat;

        auto st = file_dev.open(path, 512);
        if (!st) return st;

        std::array<std::uint8_t, 512> sector0{};
        st = file_dev.device().read(file_dev.device().ctx, 0,
            std::span<util::u8>(reinterpret_cast<util::u8*>(sector0.data()), sector0.size()));
        if (!st) return st;

        const auto lba = find_fat_partition_lba(sector0);
        part_dev.init(file_dev.device(), lba);

        st = fat.mount(part_dev.device, false);
        if (!st) return st;

        fs::clear_mounts();
        (void)fs::add_mount("/", fat.mount_point());
        return fs::Status{fs::Err::ok};
    }

    bool fs_seek_selftest(const char* path) {
        if (!path || !*path) return false;
        audio::FsDataSource src;
        if (!src.open(path)) return false;
        const auto size = src.size();
        if (!size || *size < 32) return true;
        auto pos = src.tell();
        if (!pos || *pos != 0) return false;
        if (!src.seek(0, SEEK_SET)) return false;
        pos = src.tell();
        if (!pos || *pos != 0) return false;
        if (!src.seek(16, SEEK_CUR)) return false;
        pos = src.tell();
        if (!pos || *pos != 16) return false;
        if (!src.seek(-8, SEEK_END)) return false;
        pos = src.tell();
        if (!pos || *pos != (*size - 8)) return false;
        return true;
    }

    struct UiHandles {
        WidgetHandle root{};
        WidgetHandle cover{};
        WidgetHandle title{};
        WidgetHandle subtitle{};
        WidgetHandle status{};
        WidgetHandle list{};
        WidgetHandle list_scroll{};
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
        bool duration_ready{false};
        bool ignore_list_select{false};
        bool show_debug{false};
        MenuTree menu_tree{};
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
        }

        void set_debug_visible(bool on) {
            show_debug = on;
            if (!factory) return;
            if (auto* list = factory->get_list_view(handles.list)) {
                list->set_visible(!on);
            }
            if (auto* bar = factory->get_scroll_bar(handles.list_scroll)) {
                bar->set_visible(!on);
            }
            if (auto* grid = factory->get_container(handles.debug_grid)) {
                grid->set_visible(on);
            }
        }

        void toggle_debug_view() {
            set_debug_visible(!show_debug);
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
                set_status_color({220, 120, 120, 255});
                return;
            }
            if (!player || !track_path) {
                set_status("No track");
                set_status_color({220, 120, 120, 255});
                return;
            }
            if (!track_ready) {
                set_status("Track not ready");
                set_status_color({220, 120, 120, 255});
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
            set_status_color({140, 150, 175, 255});
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
            set_status_color({230, 185, 90, 255});
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
            set_status_color({120, 200, 170, 255});
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
            set_status_color({140, 150, 175, 255});
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
                set_status_color({220, 120, 120, 255});
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
            set_status_color(track_ready ? rgba{120, 200, 170, 255} : rgba{220, 120, 120, 255});
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
                set_status_color({220, 120, 120, 255});
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
                set_status_color({220, 120, 120, 255});
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

    void on_list_draw(void* ctx, DefaultCanvas& cvs, const ListView::DrawInfo& info) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        if (info.index < 0 || info.index >= static_cast<int>(app->track_labels.size())) return;
        const Style& st = Theme::instance().get<ListView>();
        const auto font = resolve_font(st);
        Rect text = info.rect;
        text.x += st.padding;
        text.w -= st.padding * 2;
        if (text.w <= 0 || text.h <= 0) return;
        const rgba color = st.font_color;
        draw_text_box(cvs, text, app->track_labels[info.index].c_str(), color, font,
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
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
        root->set_background({18, 20, 28, 255});

        h.cover = factory.create_container();
        if (auto* cover = factory.get_container(h.cover)) {
            anchor_rect(cover, {(screen_width - kCoverSize) / 2, kUiPadding * 2, kCoverSize, kCoverSize});
            cover->set_background({40, 44, 60, 255});
        }

        h.title = factory.create_label("Beautiful Trick");
        if (auto* title = factory.get_label(h.title)) {
            title->set_color({236, 238, 246, 255});
            anchor_pos(title, kUiPadding, kUiPadding * 2 + kCoverSize + 20);
        }

        h.subtitle = factory.create_label("FELT · FLAC");
        if (auto* sub = factory.get_label(h.subtitle)) {
            sub->set_color({156, 162, 188, 255});
            anchor_pos(sub, kUiPadding, kUiPadding * 2 + kCoverSize + 46);
        }

        h.progress = factory.create_progress();
        if (auto* bar = factory.get_progress(h.progress)) {
            anchor_rect(bar, {kUiPadding, kUiPadding * 2 + kCoverSize + 90, screen_width - kUiPadding * 2, 16});
            bar->set_range(0, 100);
            bar->set_value(0);
        }

        h.time = factory.create_label("0:00 / 3:00");
        if (auto* time = factory.get_label(h.time)) {
            time->set_color({136, 142, 166, 255});
            anchor_pos(time, kUiPadding, kUiPadding * 2 + kCoverSize + 120);
        }

        h.status = factory.create_label("Stopped");
        if (auto* status = factory.get_label(h.status)) {
            status->set_color({140, 150, 175, 255});
            anchor_pos(status, kUiPadding, kUiPadding * 2 + kCoverSize + 150);
        }

        h.perf_overlay = factory.create_perf_overlay();
        if (auto* perf = factory.get_perf_overlay(h.perf_overlay)) {
            anchor_rect(perf, {screen_width - 320, kUiPadding, 300, 72});
        }

        h.debug_grid = factory.create_container();
        if (auto* grid = factory.get_container(h.debug_grid)) {
            const int list_y = kUiPadding * 2 + kCoverSize + 190;
            const int list_h = screen_height - list_y - 170;
            const int half_w = (screen_width - kUiPadding * 2 - kDemoGap) / 2;
            const int cell_h = (list_h - kDemoGap) / 2;
            anchor_rect(grid, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});
            grid->set_grid_layout(2, half_w, cell_h, kDemoGap, 0);
            grid->set_align(static_cast<int>(AlignH::Center), static_cast<int>(AlignV::Center));
            grid->set_visible(false);
        }

        h.list = factory.create_list_view();
        if (auto* list = factory.get_list_view(h.list)) {
            const int list_y = kUiPadding * 2 + kCoverSize + 190;
            const int list_h = screen_height - list_y - 170;
            anchor_rect(list, {kUiPadding, list_y, screen_width - kUiPadding * 2, list_h});
            list->set_on_draw(&on_list_draw, &ctx);
            list->set_on_select(&on_list_selected, &ctx);
            list->set_row_height(32);
            list->set_wheel_step(32);
        }

        h.list_scroll = factory.create_scroll_bar();
        if (auto* bar = factory.get_scroll_bar(h.list_scroll)) {
            const int list_y = kUiPadding * 2 + kCoverSize + 190;
            const int list_h = screen_height - list_y - 170;
            anchor_rect(bar, {screen_width - kUiPadding - 10, list_y, 10, list_h});
            bar->set_orientation(ScrollBar::Orientation::Vertical);
            bar->set_on_change(Callback{&on_list_scrollbar_change, &ctx});
        }

        h.tree = factory.create_tree_view();
        if (auto* tree = factory.get_tree_view(h.tree)) {
            tree_rebuild_visible(g_tree_demo);
            tree->set_data_source(&tree_row_count, &tree_node_info, &on_tree_draw, &g_tree_demo);
            tree->set_on_toggle(&on_tree_toggle);
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

        constexpr int button_w = 120;
        constexpr int button_h = 48;
        constexpr int gap = 12;
        const int controls_w = button_w * 3 + gap * 2;
        const int controls_h = button_h * 2 + gap;
        const int controls_x = (screen_width - controls_w) / 2;
        const int controls_y = screen_height - controls_h - 20;
        h.controls = factory.create_container();
        if (auto* controls = factory.get_container(h.controls)) {
            anchor_rect(controls, {controls_x, controls_y, controls_w, controls_h});
            controls->set_flow_layout(gap, gap, 0);
            controls->set_align(static_cast<int>(AlignH::Center), static_cast<int>(AlignV::Start));
        }

        h.btn_prev = factory.create_button("Prev");
        if (auto* prev = factory.get_button(h.btn_prev)) {
            prev->set_size(button_w, button_h);
            prev->set_on_click(Callback{&on_prev_clicked, &ctx});
        }

        h.btn_next = factory.create_button("Next");
        if (auto* next = factory.get_button(h.btn_next)) {
            next->set_size(button_w, button_h);
            next->set_on_click(Callback{&on_next_clicked, &ctx});
        }

        h.btn_play = factory.create_button("Play");
        if (auto* play = factory.get_button(h.btn_play)) {
            play->set_size(button_w, button_h);
            play->set_on_click(Callback{&on_play_clicked, &ctx});
        }

        h.btn_pause = factory.create_button("Pause");
        if (auto* pause = factory.get_button(h.btn_pause)) {
            pause->set_size(button_w, button_h);
            pause->set_on_click(Callback{&on_pause_clicked, &ctx});
        }

        h.btn_stop = factory.create_button("Stop");
        if (auto* stop = factory.get_button(h.btn_stop)) {
            stop->set_size(button_w, button_h);
            stop->set_on_click(Callback{&on_stop_clicked, &ctx});
        }

        factory.link(h.root, h.cover);
        factory.link(h.root, h.title);
        factory.link(h.root, h.subtitle);
        factory.link(h.root, h.progress);
        factory.link(h.root, h.time);
        factory.link(h.root, h.status);
        factory.link(h.root, h.list);
        factory.link(h.root, h.list_scroll);
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
    std::vector<std::string> vfs_tracks;
    vfs_tracks.emplace_back(kDefaultVfsTrack);

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

    static DefaultFrameBuffer fb;
    DefaultCanvas canvas(fb);

    static UiFactory factory;
    static audio::PlayerConfig cfg{};
    static audio::AudioPlayer player(cfg);

    PlayerUiContext ctx{};
    ctx.player = &player;
    ctx.factory = &factory;
    ctx.track_path = kDefaultVfsTrack;
    ctx.tracks = &vfs_tracks;

    auto& theme = Theme::instance();
    ThemePreset preset{};
    preset.has_label = true;
    preset.label = theme.get<Label>();
    preset.label.font_color = {230, 234, 246, 255};
    preset.has_button = true;
    preset.button = theme.get<Button>();
    preset.button.bg_color = {26, 30, 44, 255};
    preset.button.border_color = {70, 90, 120, 255};
    preset.button.padding = 6;
    preset.has_progress = true;
    preset.progress = theme.get<Progress>();
    preset.progress.bg_color = {28, 32, 46, 255};
    preset.progress.border_color = {90, 110, 150, 255};
    preset.has_perf_overlay = true;
    preset.perf_overlay = theme.get<PerfOverlay>();
    preset.perf_overlay.bg_color = {24, 26, 36, 230};
    preset.perf_overlay.border_color = {70, 90, 120, 255};
    preset.perf_overlay.font_color = {220, 228, 242, 255};
    preset.perf_overlay.padding = 6;
    apply_theme_preset(preset);

    theme.inherit<TableView, ListView>();
    theme.inherit<TreeView, ListView>();
    StylePatch table_patch{};
    table_patch.has_bg_color = true;
    table_patch.bg_color = {22, 24, 34, 255};
    table_patch.has_border_color = true;
    table_patch.border_color = {60, 70, 90, 255};
    table_patch.has_padding = true;
    table_patch.padding = 6;
    theme.patch<TableView>(table_patch);
    StylePatch tree_patch = table_patch;
    tree_patch.bg_color = {20, 22, 30, 255};
    theme.patch<TreeView>(tree_patch);

    ctx.handles = build_ui(factory, ctx);
    ctx.set_time_label(0);

    const auto mount_st = mount_fatfs_from_vhd(vhd_path);
    ctx.fs_ready = static_cast<bool>(mount_st);
    if (!ctx.fs_ready) {
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "Mount failed (%s)", fs_err_text(mount_st.err));
        ctx.set_status(buf);
        ctx.set_status_color({220, 120, 120, 255});
        ctx.mount_status = buf;
        vfs_tracks.clear();
        ctx.rebuild_track_labels();
        ctx.refresh_list_view();
        ctx.track_ready = false;
        ctx.track_path = nullptr;
        ctx.set_pause_button_text("Pause");
        ctx.set_time_label(0);
        ctx.sync_progress_value(0);
        ctx.reset_duration();
    } else {
        ctx.mount_status = "Mounted";
        vfs_tracks.clear();
        fs::Status list_st{fs::Err::ok};
        if (!collect_tracks_from_dir("/music", vfs_tracks, nullptr, list_st)) {
            std::vector<std::string> subdirs;
            const bool root_has = collect_tracks_from_dir("/", vfs_tracks, &subdirs, list_st);
            if (list_st) {
                for (const auto& dir : subdirs) {
                    collect_tracks_from_dir(dir, vfs_tracks, nullptr, list_st);
                }
            }
            if (!list_st) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "List failed (%s)", fs_err_text(list_st.err));
                ctx.set_status(buf);
                ctx.set_status_color({220, 120, 120, 255});
            } else if (!root_has && vfs_tracks.empty()) {
                ctx.set_status("No tracks found");
                ctx.set_status_color({220, 120, 120, 255});
            }
        }
        bool should_load = true;
        if (vfs_tracks.empty()) {
            if (!list_st) {
                vfs_tracks.emplace_back(kDefaultVfsTrack);
            } else {
                fs::File f{};
                auto st = fs::vfs_open(kDefaultVfsTrack, f);
                if (st) {
                    (void)fs::vfs_close(f);
                    vfs_tracks.emplace_back(kDefaultVfsTrack);
                } else {
                    char buf[64]{};
                    std::snprintf(buf, sizeof(buf), "No tracks (%s)", fs_err_text(st.err));
                    ctx.set_status(buf);
                    ctx.set_status_color({220, 120, 120, 255});
                    should_load = false;
                }
            }
        }
        ctx.rebuild_track_labels();
        ctx.refresh_list_view();
        if (should_load) {
            ctx.load_track_index(0);
            if (ctx.track_ready && !fs_seek_selftest(ctx.track_path)) {
                ctx.set_status("Fs seek selftest failed");
                ctx.set_status_color({220, 120, 120, 255});
            }
        } else {
            ctx.track_ready = false;
            ctx.track_path = nullptr;
            ctx.set_pause_button_text("Pause");
            ctx.set_time_label(0);
            ctx.sync_progress_value(0);
            ctx.reset_duration();
        }
    }

    Gui gui(canvas, factory, ctx.handles.root);

    bool running = true;
    while (running) {
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            dispatch_sdl_event(gui, ctx, evt);
        }

        player.tick();
        if (ctx.playing && !player.is_running()) {
            ctx.playing = false;
            ctx.paused = false;
            ctx.set_status("Stopped");
            ctx.set_status_color({140, 150, 175, 255});
            ctx.set_pause_button_text("Pause");
        }
        if (!ctx.paused) {
            const auto st = player.state();
            if (st == audio::PlayerState::opening) {
                ctx.set_status("Opening");
                ctx.set_status_color({140, 150, 175, 255});
            } else if (st == audio::PlayerState::buffering) {
                ctx.set_status("Buffering");
                ctx.set_status_color({140, 150, 175, 255});
            } else if (st == audio::PlayerState::playing) {
                ctx.set_status("Playing");
                ctx.set_status_color({120, 200, 170, 255});
            }
        }
        if (player.state() == audio::PlayerState::error) {
            ctx.playing = false;
            ctx.paused = false;
            const auto err = player.last_error();
            const auto stage = static_cast<audio::PlayerErrorStage>(err.ext);
            char buf[96]{};
            std::snprintf(buf, sizeof(buf), "Player error (%s/%s)",
                          audio_err_text(err.code), audio_stage_text(stage));
            ctx.set_status(buf);
            ctx.set_status_color({220, 120, 120, 255});
            ctx.set_pause_button_text("Pause");
        }
        ctx.update_duration_from_player();
        ctx.apply_pending_seek();
        ctx.update_progress();

        canvas.clear({18, 20, 28, 255});
        gui.render();

        SDL_UpdateTexture(texture, nullptr, fb.data(), screen_width * 3);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

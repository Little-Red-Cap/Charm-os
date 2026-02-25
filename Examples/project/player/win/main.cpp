#ifndef CHARM_PLAYER_DEBUG_UI
#define CHARM_PLAYER_DEBUG_UI 0
#endif

import audio.player;
import audio.result;
import player.controller;
import player.fs_utils;
import player.ui_builder;
import player.ui_debug;
import player.ui;
import charm.core.config;
import charm.core.container;
import charm.core.event;
import charm.core.factory;
import charm.core.gui;
import charm.core.input_adapter;
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
import charm.widgets.chart;
import charm.widgets.perf_overlay;
import charm.widgets.ring_indication;
import charm.widgets.text_box;
import charm.widgets.slider;
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
import charm.widgets.icon_list;
import charm.widgets.number_list;
import charm.widgets.text_tracking_list;
import charm.widgets.text_list;
import charm.widgets.progress_bar_round;
import charm.widgets.progress_bar_simple;
import charm.widgets.spin_zoom_widget;
import charm.widgets.dynamic_nebula;
import charm.widgets.crt_screen;
import charm.widgets.popup_layer;
import charm.widgets.modal_dialog;
#endif
import charm.widgets.image;
import charm.widgets.text;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import util.core;
import input.raw_event;

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
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
    constexpr const char* kDefaultVhdPath = "G:/Project/dev.vhd";
    constexpr bool kEnableSpectrumDefault = true;
    constexpr bool kLowLoadDefault = false;
    using namespace player::fs_utils;
    using namespace player::ui;
    using namespace ui::render;
#if CHARM_PLAYER_DEBUG_UI
    using namespace player::ui_debug;
#endif

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

    using PlayerUiContext = player::PlayerController;
    using UiHandles = player::UiHandles;

    static PlayerUiContext g_ctx{};

    void on_pause_clicked(void* ctx) {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        if (app->playing) app->pause_playback();
        else if (app->paused) app->resume_playback();
        else app->start_playback();
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

    void on_list_draw(void* ctx, CanvasBase& cvs, const ListView::DrawInfo& info) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        const Style& st = Theme::instance().get<ListView>();
        const auto font = resolve_font(st);
        const Rect row = info.rect;
        if (row.w <= 0 || row.h <= 0) return;

        const bool selected = info.selected;
        const rgba alt = (info.index & 1) ? rgba{26, 30, 44, 255} : rgba{22, 24, 34, 255};
        const rgba bg = selected ? rgba{32, 40, 58, 255} : alt;
        draw_rect(cvs, row.x, row.y, row.w, row.h, bg, true);
        draw_rect(cvs, row.x, row.y, row.w, 1, rgba{40, 46, 64, 255}, true);
        if (selected) {
            draw_rect(cvs, row.x, row.y, 3, row.h, kUiOk, true);
        }

        Rect text = row;
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
        const rgba color = selected ? kUiTitle : st.font_color;

        const int badge_w = 30;
        const int badge_h = row.h - 10;
        const int badge_x = text.x;
        const int badge_y = row.y + (row.h - badge_h) / 2;
        draw_round_rect(cvs, badge_x, badge_y, badge_w, badge_h, 6,
                        selected ? kUiSwitchOn : rgba{40, 48, 68, 255}, true);
        char index_buf[8]{};
        std::snprintf(index_buf, sizeof(index_buf), "%d", info.index + 1);
        Rect badge_text{badge_x, badge_y, badge_w, badge_h};
        draw_text_box(cvs, badge_text, index_buf, rgba{230, 236, 250, 255}, font,
                      TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);

        const int text_x = badge_x + badge_w + 10;
        const int text_w = text.w - (text_x - text.x);
        Rect name_rect{text_x, row.y, text_w, row.h};

        const char* ext = nullptr;
        char ext_buf[12]{};
        if (label) {
            const char* dot = std::strrchr(label, '.');
            if (dot && *(dot + 1)) {
                std::snprintf(ext_buf, sizeof(ext_buf), "%s", dot + 1);
                for (auto* p = ext_buf; *p; ++p) {
                    *p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
                }
                if (std::strlen(ext_buf) <= 6) {
                    ext = ext_buf;
                }
            }
        }

        if (ext && text_w > 80) {
            const int chip_w = 48;
            const int chip_h = badge_h - 2;
            const int chip_x = row.x + row.w - st.padding - chip_w;
            const int chip_y = row.y + (row.h - chip_h) / 2;
            draw_round_rect(cvs, chip_x, chip_y, chip_w, chip_h, 8,
                            selected ? rgba{60, 90, 140, 255} : rgba{34, 40, 56, 255}, true);
            draw_round_rect(cvs, chip_x, chip_y, chip_w, chip_h, 8, rgba{70, 90, 120, 255}, false);
            Rect chip_text{chip_x, chip_y, chip_w, chip_h};
            draw_text_box(cvs, chip_text, ext, rgba{210, 220, 240, 255}, font,
                          TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            name_rect.w = chip_x - text_x - 8;
        }

        draw_text_box(cvs, name_rect, label, color, font,
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

    void on_play_mode_click(void* ctx) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->cycle_play_mode();
    }

    void on_spectrum_toggle(void* ctx) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->on_spectrum_toggle();
    }

    void on_low_load_toggle(void* ctx) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->on_low_load_toggle();
    }

    void on_eq_toggle(void* ctx) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->on_eq_toggle();
    }

    void on_eq_slider_change(void* ctx) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->on_eq_slider_change();
    }

    void on_eq_preset_change(void* ctx) noexcept {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->on_eq_preset_change();
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

    std::optional<input::Button> map_nav_button(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return input::Button::Up;
        case SDLK_DOWN: return input::Button::Down;
        case SDLK_RETURN: return input::Button::Enter;
        case SDLK_ESCAPE: return input::Button::Back;
        case SDLK_BACKSPACE: return input::Button::Back;
        default:
            break;
        }
        return std::nullopt;
    }

    void dispatch_raw_event(Gui& gui, const input::RawInputEvent& ev) {
        const auto bridge = input::adapter::bridge_from_raw(ev);
        if (bridge.event) {
            gui.dispatch_event(*bridge.event);
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
            {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Pointer;
                raw.ms = SDL_GetTicks();
                raw.pointer = input::PointerRaw{false,
                                               static_cast<std::int16_t>(evt.motion.x),
                                               static_cast<std::int16_t>(evt.motion.y),
                                               0};
                raw.pointer_action = input::PointerAction::Move;
                dispatch_raw_event(gui, raw);
            }
            return true;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (evt.button.button == SDL_BUTTON_LEFT) {
                if (ctx.begin_drag(evt.button.x, evt.button.y)) {
                    return true;
                }
            }
            {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Pointer;
                raw.ms = SDL_GetTicks();
                raw.pointer = input::PointerRaw{true,
                                               static_cast<std::int16_t>(evt.button.x),
                                               static_cast<std::int16_t>(evt.button.y),
                                               0};
                raw.pointer_action = input::PointerAction::Down;
                dispatch_raw_event(gui, raw);
            }
            return true;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (evt.button.button == SDL_BUTTON_LEFT && ctx.dragging) {
                ctx.end_drag();
                return true;
            }
            {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Pointer;
                raw.ms = SDL_GetTicks();
                raw.pointer = input::PointerRaw{false,
                                               static_cast<std::int16_t>(evt.button.x),
                                               static_cast<std::int16_t>(evt.button.y),
                                               0};
                raw.pointer_action = input::PointerAction::Up;
                dispatch_raw_event(gui, raw);
            }
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
            if (evt.key.key == SDLK_V) {
                ctx.cycle_spectrum_style();
                return true;
            }
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = true;
                dispatch_raw_event(gui, raw);
            } else {
                gui.dispatch_event(Event::key(Event::Type::KeyDown, map_key(evt.key.key)));
            }
            return true;
        case SDL_EVENT_KEY_UP:
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = false;
                dispatch_raw_event(gui, raw);
            } else {
                gui.dispatch_event(Event::key(Event::Type::KeyUp, map_key(evt.key.key)));
            }
            return true;
        default:
            return false;
        }
    }


}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    const char* vhd_path = kDefaultVhdPath;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Charm Player", screen_width, screen_height, SDL_WINDOW_RESIZABLE);
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
    g_ctx.set_spectrum_enabled(kEnableSpectrumDefault);
    g_ctx.set_low_load(kLowLoadDefault);

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

    player::UiCallbacks ui_cb{};
    ui_cb.list_draw = &on_list_draw;
    ui_cb.list_select = &on_list_selected;
    ui_cb.list_pool_create = &on_list_pool_create;
    ui_cb.list_pool_bind = &on_list_pool_bind;
    ui_cb.list_pool_recycle = &on_list_pool_recycle;
    ui_cb.list_ctx = &g_ctx;
    ui_cb.list_scroll_change = Callback{&on_list_scrollbar_change, &g_ctx};
    ui_cb.spectrum_toggle = Callback{&on_spectrum_toggle, &g_ctx};
    ui_cb.low_load_toggle = Callback{&on_low_load_toggle, &g_ctx};
    ui_cb.eq_toggle = Callback{&on_eq_toggle, &g_ctx};
    ui_cb.eq_slider_change = Callback{&on_eq_slider_change, &g_ctx};
    ui_cb.eq_preset_change = Callback{&on_eq_preset_change, &g_ctx};
    ui_cb.prev_click = Callback{&on_prev_clicked, &g_ctx};
    ui_cb.next_click = Callback{&on_next_clicked, &g_ctx};
    ui_cb.mode_click = Callback{&on_play_mode_click, &g_ctx};
    ui_cb.pause_click = Callback{&on_pause_clicked, &g_ctx};

    g_ctx.handles = build_ui(g_factory, g_ctx, ui_cb);
    g_ctx.sync_option_states();
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
    g_ctx.set_play_button_icon(false);
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
        g_ctx.set_play_button_icon(false);
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

    int win_w = screen_width;
    int win_h = screen_height;
    bool running = true;
    while (running) {
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
                win_w = static_cast<int>(evt.window.data1);
                win_h = static_cast<int>(evt.window.data2);
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
                g_ctx.set_play_button_icon(false);
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
            g_ctx.set_play_button_icon(false);
        }
        g_ctx.update_duration_from_player();
        g_ctx.apply_pending_seek();
        bool needs_redraw = false;
        needs_redraw |= g_ctx.update_progress();
        needs_redraw |= g_ctx.update_spectrum();

        if (g_ctx.playing) {
            const auto now = std::chrono::steady_clock::now();
            const auto ms = static_cast<std::uint32_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time).count());
            const float phase = static_cast<float>(ms % 2000) / 2000.0f;
            const float v = 0.5f - 0.5f * std::cos(phase * 6.2831853f);
            apply_cover_pulse(&g_ctx, v);
            needs_redraw = true;
        }

        if (g_player.state() == audio::PlayerState::opening ||
            g_player.state() == audio::PlayerState::buffering) {
            needs_redraw = true;
        }

        if (needs_redraw) {
            gui->invalidate_cache();
        }

        g_canvas.clear(kUiBackground);
        gui->render();

        SDL_UpdateTexture(texture, nullptr, g_framebuffer.data(), screen_width * 3);
        SDL_RenderClear(renderer);
        SDL_FRect dst{
            0.0f,
            0.0f,
            static_cast<float>(win_w),
            static_cast<float>(win_h)
        };
        SDL_RenderTexture(renderer, texture, nullptr, &dst);
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

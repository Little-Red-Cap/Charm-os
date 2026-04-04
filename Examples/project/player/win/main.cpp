import audio.player;
import audio.result;
import player.app;
import player.controller;
import player.fs_utils;
import player.platform;
import player.storage;
import player.playback;
import player.ui_builder;
import player.ui;
import charm.core.config;
import charm.core.event;
import charm.ui.scene;
import ui.input_adapter;
import charm.gfx.color;
import charm.gfx.text_box;
import charm.gfx.image;
import charm.gfx.snapshot;
import charm.font.typography;
import charm.font.font_noto_ascii_16;
import charm.font.font_noto_sc_16;
import fs_core;
import fs_errno;
import fs_stream;
import fs_vfs;
import charm.system.clock;
import charm.system.run_loop;
import util.core;
import input.raw_event;
import platform.win.time_source;

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#include <SDL3/SDL.h>
#if defined(_WIN32)
#undef NOMINMAX
#undef WIN32_LEAN_AND_MEAN
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
    using namespace player::fs_utils;
    using namespace player::ui;

    static charm::system::ClockTick now_us(void*) noexcept {
        return platform::win::SteadyClock::now();
    }

    static player::PlayerPlatform g_platform{};
    static audio::PlayerConfig g_player_cfg{};
    static charm::system::Clock g_clock{nullptr, {.now_us = &now_us}};
    static std::optional<player::App> g_app{};

    using PlayerUiContext = player::PlayerController;
    using UiHandles = player::UiHandles;

    static PlayerUiContext g_ctx{};
    static std::array<float, 24> g_spectrum{};

    void update_spectrum(float t_sec, bool active) {
        const float base_speed = 2.2f;
        for (std::size_t i = 0; i < g_spectrum.size(); ++i) {
            const float phase = t_sec * base_speed + static_cast<float>(i) * 0.35f;
            const float wave = 0.5f + 0.5f * std::sin(phase);
            const float target = active ? (0.12f + wave * 0.88f) : 0.05f;
            g_spectrum[i] = g_spectrum[i] * 0.82f + target * 0.18f;
        }
    }

    void draw_library_fx(::ui::scene::SceneOverlay& out,
                         const PlayerUiContext& ctx,
                         const ::ui::scene::Scene& scene) {
        if (ctx.current_page != player::PlayerPage::Library) {
            return;
        }
        if (!ctx.handles.page_library || scene.world_rect(ctx.handles.page_library).w <= 0) {
            return;
        }
        (void)scene;
    }

    void draw_now_playing_fx(::ui::scene::SceneOverlay& out,
                             const PlayerUiContext& ctx,
                             const ::ui::scene::Scene& scene,
                             float t_sec) {
        if (ctx.current_page != player::PlayerPage::NowPlaying) {
            return;
        }
        if (!ctx.handles.page_now_playing || scene.world_rect(ctx.handles.page_now_playing).w <= 0) {
            return;
        }
        const Rect cover = scene.world_rect(ctx.handles.cover);
        const int cover_radius = 20;
        const auto cover_image = ctx.cover_image.image_id;
        if (!ui::gfx::image_id_valid(cover_image)) {
            out.fill_round_rect(cover, cover_radius, kUiCover);
        }
        rgba cover_ring = kUiOk;
        if (ctx.is_playing()) {
            const float pulse = 0.4f + 0.6f * std::sin(t_sec * 2.0f);
            cover_ring.a = static_cast<std::uint8_t>(80 + pulse * 120.0f);
        } else {
            cover_ring.a = 70;
        }
        out.stroke_round_rect(cover, cover_radius, cover_ring);
        (void)scene;

        const Rect spec = scene.world_rect(ctx.handles.spectrum);
        if (spec.w > 0 && spec.h > 0) {
            out.fill_round_rect(spec, 10, kUiListBg);
            out.stroke_round_rect(spec, 10, kUiListBorder);
            const int bar_count = static_cast<int>(g_spectrum.size());
            const int gap = 2;
            const int bar_w = std::max(2, (spec.w - gap * (bar_count - 1)) / bar_count);
            int x = spec.x;
            for (int i = 0; i < bar_count; ++i) {
                const float v = g_spectrum[static_cast<std::size_t>(i)];
                const int h = static_cast<int>(v * static_cast<float>(spec.h - 8));
                const int y = spec.y + spec.h - 4 - h;
                const Rect bar{ x, y, bar_w, h };
                out.fill_round_rect(bar, 3, kUiSwitchOn);
                x += bar_w + gap;
            }
        }

        const Rect progress = scene.world_rect(ctx.handles.progress);
        if (progress.w > 0 && progress.h > 0) {
            const int wave_y = progress.y + progress.h / 2;
            const int dot_r = 2;
            const int dot_gap = 6;
            const int dot_count = std::max(8, progress.w / (dot_r * 2 + dot_gap));
            const int phase = static_cast<int>(t_sec * 8.0f) % (dot_r * 2 + dot_gap);
            int x = progress.x + 8 + phase;
            for (int i = 0; i < dot_count; ++i) {
                const int y = wave_y + ((i % 4 == 0) ? -1 : 0);
                out.fill_circle(Rect{x - dot_r, y - dot_r, dot_r * 2, dot_r * 2}, kUiTimeSoft);
                x += dot_r * 2 + dot_gap;
                if (x > progress.x + progress.w - 6) break;
            }
        }
    }

    struct PlayerLoopState;

    struct PixelBounds {
        bool found{false};
        int min_x{0};
        int min_y{0};
        int max_x{0};
        int max_y{0};
        int count{0};
    };

    struct GlyphInkSpan {
        bool found{false};
        int first_row{0};
        int last_row{0};
        int rows{0};
    };

    int color_delta(const rgba& a, const rgba& b) noexcept {
        return std::abs(static_cast<int>(a.r) - static_cast<int>(b.r))
             + std::abs(static_cast<int>(a.g) - static_cast<int>(b.g))
             + std::abs(static_cast<int>(a.b) - static_cast<int>(b.b));
    }

    rgba sample_label_bg(player::PlayerPlatform& platform, const Rect& rect) noexcept {
        const auto& fb = platform.framebuffer_ref();
        const int x = std::clamp(rect.x + rect.w - 4, 0, screen_width - 1);
        const int y = std::clamp(rect.y + rect.h / 2, 0, screen_height - 1);
        return fb.get_pixel(static_cast<std::size_t>(x), static_cast<std::size_t>(y));
    }

    PixelBounds scan_text_pixels(player::PlayerPlatform& platform,
                                 const Rect& rect,
                                 const rgba& bg) noexcept {
        PixelBounds out{};
        const auto& fb = platform.framebuffer_ref();
        const int x0 = std::max(0, rect.x);
        const int y0 = std::max(0, rect.y);
        const int x1 = std::min(screen_width, rect.x + rect.w);
        const int y1 = std::min(screen_height, rect.y + rect.h);
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                const rgba px = fb.get_pixel(static_cast<std::size_t>(x), static_cast<std::size_t>(y));
                if (color_delta(px, bg) <= 18) continue;
                if (!out.found) {
                    out.found = true;
                    out.min_x = out.max_x = x;
                    out.min_y = out.max_y = y;
                } else {
                    out.min_x = std::min(out.min_x, x);
                    out.min_y = std::min(out.min_y, y);
                    out.max_x = std::max(out.max_x, x);
                    out.max_y = std::max(out.max_y, y);
                }
                ++out.count;
            }
        }
        return out;
    }

    GlyphInkSpan inspect_glyph_ink(const Glyph& glyph) noexcept {
        GlyphInkSpan out{};
        if (!glyph.bitmap || glyph.width <= 0 || glyph.height <= 0) return out;
        const std::uint8_t bpp = glyph.bpp ? glyph.bpp : 1;
        const int bytes_per_row = (glyph.width * bpp + 7) / 8;
        for (int row = 0; row < glyph.height; ++row) {
            bool row_has_ink = false;
            for (int col = 0; col < glyph.width; ++col) {
                std::uint8_t cov = 0;
                if (bpp == 1) {
                    const int byte_index = row * bytes_per_row + col / 8;
                    const int bit_offset = 7 - (col % 8);
                    cov = ((glyph.bitmap[byte_index] >> bit_offset) & 1) ? 255 : 0;
                } else if (bpp == 2) {
                    const int byte_index = row * bytes_per_row + col / 4;
                    const int shift = (3 - (col % 4)) * 2;
                    const std::uint8_t v = (glyph.bitmap[byte_index] >> shift) & 0x03;
                    cov = static_cast<std::uint8_t>((v * 255) / 3);
                } else if (bpp == 4) {
                    const int byte_index = row * bytes_per_row + col / 2;
                    const int shift = (1 - (col % 2)) * 4;
                    const std::uint8_t v = (glyph.bitmap[byte_index] >> shift) & 0x0F;
                    cov = static_cast<std::uint8_t>((v * 255) / 15);
                } else if (bpp == 8) {
                    const int byte_index = row * bytes_per_row + col;
                    cov = glyph.bitmap[byte_index];
                }
                if (cov) {
                    row_has_ink = true;
                    break;
                }
            }
            if (!row_has_ink) continue;
            if (!out.found) {
                out.found = true;
                out.first_row = out.last_row = row;
            } else {
                out.last_row = row;
            }
            ++out.rows;
        }
        return out;
    }

    void log_font_box(const char* name,
                      const char* text,
                      const Font& font,
                      const Rect& rect,
                      player::PlayerPlatform& platform) {
        if (!name || !text || rect.w <= 0 || rect.h <= 0) {
            return;
        }
        const rgba bg = sample_label_bg(platform, rect);
        const PixelBounds box = scan_text_pixels(platform, rect, bg);
        const int measured_w = measure_text_width(text, font);
        const auto first = text[0]
            ? resolve_glyph_fallback(font, static_cast<std::uint32_t>(static_cast<unsigned char>(text[0])))
            : ResolvedGlyph{};
        const Glyph empty_glyph{};
        const Glyph* glyph = first.glyph ? first.glyph : &empty_glyph;
        const GlyphInkSpan ink = inspect_glyph_ink(*glyph);
        const int bbox_w = box.found ? (box.max_x - box.min_x + 1) : 0;
        const int bbox_h = box.found ? (box.max_y - box.min_y + 1) : 0;
        std::printf(
            "[font-probe] name=%s text=%s rect=%d,%d,%d,%d bg=%u,%u,%u line=%d base=%d measure=%d bbox=%d,%d,%d,%d size=%dx%d pixels=%d glyph=%dx%d adv=%d xoff=%d yoff=%d ink=%d..%d rows=%d\n",
            name,
            text,
            rect.x,
            rect.y,
            rect.w,
            rect.h,
            bg.r,
            bg.g,
            bg.b,
            font.line_height,
            font.baseline,
            measured_w,
            box.min_x,
            box.min_y,
            box.max_x,
            box.max_y,
            bbox_w,
            bbox_h,
            box.count,
            glyph->width,
            glyph->height,
            glyph->x_advance,
            glyph->x_offset,
            glyph->y_offset,
            ink.first_row,
            ink.last_row,
            ink.rows);
    }

    void dump_font_probe_metrics(const PlayerLoopState& state);

    struct PlayerLoopState {
        player::App* app{nullptr};
        player::PlayerPlatform* platform{nullptr};
        PlayerUiContext* ctx{nullptr};
        ::ui::scene::Scene* scene{nullptr};
        SDL_Renderer* renderer{nullptr};
        SDL_Texture* texture{nullptr};
        bool* running{nullptr};
        int* win_w{nullptr};
        int* win_h{nullptr};
        float t_sec{0.0f};
        std::string screenshot_path{};
        std::string screenshot_gif_path{};
        player::PlayerPage screenshot_page{player::PlayerPage::Library};
        bool screenshot_verbose{false};
        int screenshot_wait_frames{0};
        bool screenshot_exit{false};
    };

    struct UiCiResult {
        bool ok{true};
        int failed{0};
    };

    void dump_font_probe_metrics(const PlayerLoopState& state) {
        if (!state.platform || !state.scene || !state.ctx) return;
        const auto& scene = *state.scene;
        using namespace player::ui;
        const Font& hero_font = get_player_font_px(72, FontWeight::Bold);
        const Font& subtitle_font = get_player_font_px(18, FontWeight::Regular);
        const Font& page_title_font = get_font_weighted(FontId::Normal, FontWeight::Bold);
        const Font& meta_font = get_font_weighted(FontId::Small, FontWeight::Regular);
        if (state.ctx->current_page == player::PlayerPage::Probe) {
            log_font_box("probe.hero_top", "Your", hero_font,
                         scene.world_rect(state.ctx->handles.probe_hero_top), *state.platform);
            log_font_box("probe.hero_bottom", "Mix", hero_font,
                         scene.world_rect(state.ctx->handles.probe_hero_bottom), *state.platform);
            log_font_box("probe.hero_subtitle", "Today's Mix for you", subtitle_font,
                         scene.world_rect(state.ctx->handles.probe_hero_subtitle), *state.platform);
            log_font_box("probe.ref_title", "Reference title", page_title_font,
                         scene.world_rect(state.ctx->handles.probe_ref_title), *state.platform);
            log_font_box("probe.ref_meta", "Meta / body baseline sample", meta_font,
                         scene.world_rect(state.ctx->handles.probe_ref_meta), *state.platform);
        } else if (state.ctx->current_page == player::PlayerPage::Home) {
            log_font_box("home.hero_top", "Your", hero_font,
                         scene.world_rect(state.ctx->handles.home_title_top), *state.platform);
            log_font_box("home.hero_bottom", "Mix", hero_font,
                         scene.world_rect(state.ctx->handles.home_title_bottom), *state.platform);
            log_font_box("home.hero_subtitle", "Today's Mix for you", subtitle_font,
                         scene.world_rect(state.ctx->handles.home_subtitle), *state.platform);
        }
    }

    constexpr std::size_t kUiCmdBudget = 1200;
    constexpr std::uint64_t kUiAlphaBlendBudget = 1000000;

    void ui_ci_emit(const char* name, bool ok, const char* reason) {
        if (ok) {
            std::printf("[ui-ci] case=%s ok=1\n", name);
        } else {
            std::printf("[ui-ci] case=%s ok=0 reason=%s\n", name, reason ? reason : "unknown");
        }
    }

    void ui_ci_click(player::App& app, PlayerUiContext& ctx, ::ui::scene::Scene& scene, int x, int y) {
        input::RawInputEvent down{};
        down.type = input::RawInputEventType::Pointer;
        down.ms = 0;
        down.pointer = input::PointerRaw{true, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), 0};
        down.pointer_action = input::PointerAction::Down;
        app.dispatch_raw_input(scene, ctx, down);

        input::RawInputEvent up{};
        up.type = input::RawInputEventType::Pointer;
        up.ms = 0;
        up.pointer = input::PointerRaw{false, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y), 0};
        up.pointer_action = input::PointerAction::Up;
        app.dispatch_raw_input(scene, ctx, up);
    }

    UiCiResult run_ui_ci(player::App& app, PlayerUiContext& ctx, player::PlayerPlatform& platform) {
        UiCiResult res{};
        auto& scene = platform.scene_ref();

        platform.begin_frame();
        platform.render();
        platform.end_frame();

        {
            const auto cmd_stats = scene.last_cmd_stats();
            const auto exec_stats = scene.last_exec_stats();
            if (cmd_stats.cmd_count > kUiCmdBudget) {
                ui_ci_emit("frame_budget_cmd", false, "cmd_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_cmd", true, nullptr);
            }
            if (exec_stats.alpha_blend_count > kUiAlphaBlendBudget) {
                ui_ci_emit("frame_budget_alpha", false, "alpha_budget");
                res.ok = false;
                res.failed++;
            } else {
                ui_ci_emit("frame_budget_alpha", true, nullptr);
            }
        }

        auto click_handle = [&](WidgetHandle h, const char* case_name) -> bool {
            if (!h) {
                ui_ci_emit(case_name, false, "invalid_handle");
                res.ok = false;
                res.failed++;
                return false;
            }
            const Rect r = scene.world_rect(h);
            if (r.w <= 0 || r.h <= 0) {
                ui_ci_emit(case_name, false, "zero_rect");
                res.ok = false;
                res.failed++;
                return false;
            }
            const int cx = r.x + r.w / 2;
            const int cy = r.y + r.h / 2;
            ui_ci_click(app, ctx, scene, cx, cy);
            return true;
        };

        ctx.set_page(player::PlayerPage::Library);
        if (click_handle(ctx.handles.nav_home, "library_to_home")) {
            if (ctx.current_page == player::PlayerPage::Home) {
                ui_ci_emit("library_to_home", true, nullptr);
            } else {
                ui_ci_emit("library_to_home", false, "page_not_home");
                res.ok = false;
                res.failed++;
            }
        }

        ctx.set_page(player::PlayerPage::Home);
        if (click_handle(ctx.handles.bottom_hit, "home_to_now")) {
            if (ctx.current_page == player::PlayerPage::NowPlaying) {
                ui_ci_emit("home_to_now", true, nullptr);
            } else {
                ui_ci_emit("home_to_now", false, "page_not_now");
                res.ok = false;
                res.failed++;
            }
        }

        if (click_handle(ctx.handles.now_back, "now_back_to_home")) {
            if (ctx.current_page == player::PlayerPage::Home) {
                ui_ci_emit("now_back_to_home", true, nullptr);
            } else {
                ui_ci_emit("now_back_to_home", false, "page_not_home");
                res.ok = false;
                res.failed++;
            }
        }

        const auto* tracks = ctx.storage.tracks;
        if (tracks && tracks->size() > 0) {
            ctx.set_page(player::PlayerPage::Library);
            const Rect list = scene.world_rect(ctx.handles.list);
            if (list.w > 0 && list.h > 0) {
                const int before = ctx.last_list_selected;
                ui_ci_click(app, ctx, scene, list.x + 12, list.y + 12);
                const int after = ctx.last_list_selected;
                if (after >= 0) {
                    ui_ci_emit("list_select", true, nullptr);
                } else {
                    const char* reason = (before == after) ? "no_change" : "no_select";
                    ui_ci_emit("list_select", false, reason);
                    res.ok = false;
                    res.failed++;
                }
            } else {
                ui_ci_emit("list_select", false, "list_rect_zero");
                res.ok = false;
                res.failed++;
            }
        } else {
            ui_ci_emit("list_select", true, "skipped_no_tracks");
        }

        std::printf("[ui-ci] done ok=%d failed=%d\n", res.ok ? 1 : 0, res.failed);
        return res;
    }

    bool dispatch_sdl_event(::ui::scene::Scene& scene,
                            player::App& app,
                            PlayerUiContext& ctx,
                            const SDL_Event& evt);

    void loop_poll_events(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->running || !state->app || !state->ctx || !state->platform) {
            return;
        }
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                *state->running = false;
                break;
            }
            if (evt.type == SDL_EVENT_WINDOW_RESIZED) {
                if (state->win_w) {
                    *state->win_w = static_cast<int>(evt.window.data1);
                }
                if (state->win_h) {
                    *state->win_h = static_cast<int>(evt.window.data2);
                }
            }
            dispatch_sdl_event(state->platform->scene_ref(), *state->app, *state->ctx, evt);
        }
    }

    void loop_update(void* ctx, charm::system::ClockTick now_us, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->app || !state->ctx) {
            return;
        }
        state->t_sec = static_cast<float>(now_us) * 0.000001f;
        state->app->tick();
        state->ctx->tick_player(state->app->player());
        update_spectrum(state->t_sec, state->ctx->is_playing());
    }

    void loop_render(void* ctx, charm::system::ClockTick, charm::system::ClockTick) noexcept {
        auto* state = static_cast<PlayerLoopState*>(ctx);
        if (!state || !state->platform || !state->ctx || !state->scene || !state->renderer || !state->texture) {
            return;
        }
        if (!state->screenshot_path.empty() || !state->screenshot_gif_path.empty()) {
            if (state->ctx->current_page != state->screenshot_page) {
                state->ctx->set_page(state->screenshot_page);
                if (state->screenshot_wait_frames < 1) state->screenshot_wait_frames = 1;
            }
        }
        state->platform->framebuffer_ref().clear(kUiBackground);
        state->platform->begin_frame();
        state->platform->scene_ref().set_overlay(
            [](::ui::scene::SceneOverlay& overlay, void* ctx) noexcept {
                auto* state = static_cast<PlayerLoopState*>(ctx);
                if (!state || !state->ctx || !state->scene) return;
                draw_library_fx(overlay, *state->ctx, *state->scene);
                draw_now_playing_fx(overlay, *state->ctx, *state->scene, state->t_sec);
            },
            state);
        state->platform->render();
        state->platform->end_frame();

        SDL_UpdateTexture(state->texture,
                          nullptr,
                          state->platform->canvas_ref().data(),
                          static_cast<int>(state->platform->stride_bytes()));
        SDL_RenderClear(state->renderer);
        SDL_RenderTexture(state->renderer, state->texture, nullptr, nullptr);
        SDL_RenderPresent(state->renderer);

        if (!state->screenshot_path.empty() || !state->screenshot_gif_path.empty()) {
            if (state->screenshot_wait_frames > 0) {
                state->screenshot_wait_frames--;
                return;
            }
            auto& fb = state->platform->framebuffer_ref();
            ::FrameBufferView view{
                screen_pixel_format,
                fb.data(),
                static_cast<std::size_t>(screen_width),
                static_cast<std::size_t>(screen_height),
                state->platform->stride_bytes()
            };
            if (state->screenshot_verbose) {
                dump_font_probe_metrics(*state);
            }
            if (!state->screenshot_path.empty()) {
                const bool ok = ::charm::gfx::snapshot::write_ppm(state->screenshot_path.c_str(), view);
                if (state->screenshot_verbose) {
                    std::printf("[ui] screenshot ppm=%s ok=%d\n", state->screenshot_path.c_str(), ok ? 1 : 0);
                }
                state->screenshot_path.clear();
            }
            if (!state->screenshot_gif_path.empty()) {
                std::vector<std::vector<std::uint8_t>> frames{};
                frames.push_back(::charm::gfx::snapshot::capture_indexed_332(view));
                const bool ok = ::charm::gfx::snapshot::write_gif(state->screenshot_gif_path.c_str(),
                                                                  static_cast<int>(view.width),
                                                                  static_cast<int>(view.height),
                                                                  frames,
                                                                  8);
                if (state->screenshot_verbose) {
                    std::printf("[ui] screenshot gif=%s ok=%d\n", state->screenshot_gif_path.c_str(), ok ? 1 : 0);
                }
                state->screenshot_gif_path.clear();
            }
            if (state->screenshot_exit
                && state->screenshot_path.empty()
                && state->screenshot_gif_path.empty()
                && state->running) {
                *state->running = false;
            }
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

    std::optional<player::UiKey> map_ui_key(SDL_Keycode key) noexcept {
        switch (key) {
        case SDLK_UP: return player::UiKey::Up;
        case SDLK_DOWN: return player::UiKey::Down;
        case SDLK_RETURN: return player::UiKey::Enter;
        case SDLK_SPACE: return player::UiKey::PlayToggle;
        case SDLK_N: return player::UiKey::Next;
        case SDLK_P: return player::UiKey::Prev;
        case SDLK_M: return player::UiKey::Mode;
        default:
            break;
        }
        return std::nullopt;
    }

    bool dispatch_sdl_event(::ui::scene::Scene& scene, player::App& app, PlayerUiContext& ctx, const SDL_Event& evt) {
        switch (evt.type) {
        case SDL_EVENT_MOUSE_MOTION: {
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = SDL_GetTicks();
            raw.pointer = input::PointerRaw{false,
                                            static_cast<std::int16_t>(evt.motion.x),
                                            static_cast<std::int16_t>(evt.motion.y),
                                            0};
            raw.pointer_action = input::PointerAction::Move;
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            if (evt.button.button != SDL_BUTTON_LEFT) return true;
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = SDL_GetTicks();
            raw.pointer = input::PointerRaw{true,
                                            static_cast<std::int16_t>(evt.button.x),
                                            static_cast<std::int16_t>(evt.button.y),
                                            0};
            raw.pointer_action = input::PointerAction::Down;
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            if (evt.button.button != SDL_BUTTON_LEFT) return true;
            input::RawInputEvent raw{};
            raw.type = input::RawInputEventType::Pointer;
            raw.ms = SDL_GetTicks();
            raw.pointer = input::PointerRaw{false,
                                            static_cast<std::int16_t>(evt.button.x),
                                            static_cast<std::int16_t>(evt.button.y),
                                            0};
            raw.pointer_action = input::PointerAction::Up;
            app.dispatch_raw_input(scene, ctx, raw);
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL:
            {
                float mx = 0.0f;
                float my = 0.0f;
                SDL_GetMouseState(&mx, &my);
                scene.dispatch_event(Event::wheel(static_cast<int>(mx),
                                                  static_cast<int>(my),
                                                  evt.wheel.y));
            }
            ctx.process_input_events();
            return true;
        case SDL_EVENT_KEY_DOWN:
            if (auto k = map_ui_key(evt.key.key)) {
                ctx.handle_key_action(*k);
            }
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = true;
                app.dispatch_raw_input(scene, ctx, raw);
            }
            return true;
        case SDL_EVENT_KEY_UP:
            if (auto b = map_nav_button(evt.key.key)) {
                input::RawInputEvent raw{};
                raw.type = input::RawInputEventType::Button;
                raw.ms = SDL_GetTicks();
                raw.button = *b;
                raw.pressed = false;
                app.dispatch_raw_input(scene, ctx, raw);
            }
            return true;
        default:
            return false;
        }
    }
}

int main(int argc, char** argv) {
    std::string screenshot_path{};
    std::string screenshot_gif_path{};
    bool screenshot_verbose = false;
    int screenshot_wait_frames = 0;
    bool screenshot_exit = false;
    player::PlayerPage start_page = player::PlayerPage::Home;
    bool start_page_set = false;
    bool ui_ci = false;
    std::string font_ttf_path{};
    int font_small_px = 0;
    int font_normal_px = 0;
    int font_large_px = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i] ? argv[i] : "";
        if (arg.rfind("--screenshot=", 0) == 0) {
            screenshot_path.assign(arg.substr(13));
        } else if (arg.rfind("--screenshot-gif=", 0) == 0) {
            screenshot_gif_path.assign(arg.substr(17));
        } else if (arg.rfind("--screenshot-page=", 0) == 0) {
            const std::string_view page = arg.substr(18);
            if (page == "probe") {
                start_page = player::PlayerPage::Probe;
                start_page_set = true;
            } else if (page == "home") {
                start_page = player::PlayerPage::Home;
                start_page_set = true;
            } else if (page == "now") {
                start_page = player::PlayerPage::NowPlaying;
                start_page_set = true;
            } else if (page == "library") {
                start_page = player::PlayerPage::Library;
                start_page_set = true;
            }
        } else if (arg.rfind("--page=", 0) == 0) {
            const std::string_view page = arg.substr(7);
            if (page == "probe") {
                start_page = player::PlayerPage::Probe;
                start_page_set = true;
            } else if (page == "home") {
                start_page = player::PlayerPage::Home;
                start_page_set = true;
            } else if (page == "now") {
                start_page = player::PlayerPage::NowPlaying;
                start_page_set = true;
            } else if (page == "library") {
                start_page = player::PlayerPage::Library;
                start_page_set = true;
            }
        } else if (arg == "--screenshot-verbose") {
            screenshot_verbose = true;
        } else if (arg == "--screenshot-exit") {
            screenshot_exit = true;
        } else if (arg.rfind("--screenshot-frame=", 0) == 0) {
            const std::string_view value = arg.substr(19);
            screenshot_wait_frames = std::max(0, std::atoi(std::string(value).c_str()));
        } else if (arg == "--ui-ci") {
            ui_ci = true;
        } else if (arg.rfind("--font-ttf=", 0) == 0) {
            font_ttf_path.assign(arg.substr(11));
        } else if (arg.rfind("--font-small=", 0) == 0) {
            font_small_px = std::max(0, std::atoi(std::string(arg.substr(13)).c_str()));
        } else if (arg.rfind("--font-normal=", 0) == 0) {
            font_normal_px = std::max(0, std::atoi(std::string(arg.substr(14)).c_str()));
        } else if (arg.rfind("--font-large=", 0) == 0) {
            font_large_px = std::max(0, std::atoi(std::string(arg.substr(13)).c_str()));
        }
    }
    if (!start_page_set && (!screenshot_path.empty() || !screenshot_gif_path.empty())) {
        start_page = player::PlayerPage::NowPlaying;
    }

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
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

    g_player_cfg.output_mode = audio::OutputMode::fixed_rate;
    g_player_cfg.fixed_rate = 48000;
    charm::system::ClockCaps::TimeSource::bind(g_clock);
    player::AppConfig app_cfg{g_player_cfg};
    if (!font_ttf_path.empty()) {
        app_cfg.ttf_path = font_ttf_path;
    } else {
        app_cfg.ttf_path = "/font/gflex_variable.ttf";
    }
    if (font_small_px > 0) {
        app_cfg.ttf_small_px = font_small_px;
    }
    if (font_normal_px > 0) {
        app_cfg.ttf_normal_px = font_normal_px;
    }
    if (font_large_px > 0) {
        app_cfg.ttf_large_px = font_large_px;
    }
    g_app.emplace(std::move(app_cfg), g_clock);

    player::init_storage(player::default_storage_config());
    g_app->bind_player(g_ctx);
    g_ctx.bind_scene(g_platform.scene_ref());
    g_ctx.set_start_page(start_page);
    (void)g_app->scan_storage();
    g_ctx.apply_storage_view(g_app->storage_view());
    g_platform.build_scene([&](::ui::scene::SceneBuilder& builder) {
        g_app->bind_ui(builder, g_ctx);
    });
    g_ctx.set_page(start_page);

    const bool has_track = g_app->bootstrap_player(g_ctx, 0, false);
    if (has_track && !fs_seek_selftest(g_ctx.track_path())) {
        g_ctx.set_status("Fs seek selftest failed");
    }

    if (ui_ci) {
        const UiCiResult result = run_ui_ci(*g_app, g_ctx, g_platform);
        g_app->shutdown();
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return result.ok ? 0 : 2;
    }

    int win_w = screen_width;
    int win_h = screen_height;
    bool running = true;
    PlayerLoopState loop_state{
        .app = &(*g_app),
        .platform = &g_platform,
        .ctx = &g_ctx,
        .scene = &g_platform.scene_ref(),
        .renderer = renderer,
        .texture = texture,
        .running = &running,
        .win_w = &win_w,
        .win_h = &win_h,
        .screenshot_path = std::move(screenshot_path),
        .screenshot_gif_path = std::move(screenshot_gif_path),
        .screenshot_page = start_page,
        .screenshot_verbose = screenshot_verbose,
        .screenshot_wait_frames = screenshot_wait_frames,
        .screenshot_exit = screenshot_exit
    };
    charm::system::RunLoop<4> loop{};
    loop.bind_clock(g_clock);
    (void)loop.add_step(charm::system::LoopPhase::io, charm::system::SubmitProjection::event, &loop_poll_events, &loop_state, "player_io");
    (void)loop.add_step(charm::system::LoopPhase::update, charm::system::SubmitProjection::event, &loop_update, &loop_state, "player_update");
    (void)loop.add_step(charm::system::LoopPhase::render, charm::system::SubmitProjection::event, &loop_render, &loop_state, "player_render");
    std::array<char, 384> run_loop_audit{};
    (void)loop.format_audit_json(run_loop_audit.data(), run_loop_audit.size());
    std::printf("[runloop.audit] %s\n", run_loop_audit.data());
    while (running) {
        loop.run_once();
        (void)win_w;
        (void)win_h;
    }

    g_app->shutdown();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}


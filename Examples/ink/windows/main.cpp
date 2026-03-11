#include <chrono>
#include <cmath>
#include <cstdio>
#include <optional>
#include <thread>

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_log.h"
#include "SDL3/SDL.h"

import gui.canvas_1bpp;
import gui.renderer;
import gui.perf;
import gui.core;
import gui.motion;
import gui.ui_settings;
import app.state;
import app.theme;
import app.ui;
import app.logic;
import app.logic_intent;
import app.runtime;
import gui.ui_input_policy;

import gui.input;
import input.router;
import input.source.sdl;

import backend.sdl3;
import debug.scope;

#if defined(UI_SEM_TEST) && UI_SEM_TEST
import gui.ui_semantics.tests;
#endif


int main() try {
#if defined(UI_SEM_TEST) && UI_SEM_TEST
    run_ui_semantics_tests();
    return 0;
#endif
    using Canvas = gui::Canvas1bpp<128, 64>;
    Canvas canvas_a;
    std::optional<Canvas> canvas_b;
    gui::Renderer<Canvas> renderer_a(canvas_a);
    std::optional<gui::Renderer<Canvas>> renderer_b;

    constexpr int kScale = 8;
    const char* kTitleA = "Charm-ink A";
    const char* kTitleB = "Charm-ink B";
    constexpr bool kEnableSecond = false;
    backend::SDL3Backend<128, 64> sdl_a(kTitleA, kScale);
    std::optional<backend::SDL3Backend<128, 64>> sdl_b;
    if (kEnableSecond) {
        sdl_b.emplace(kTitleB, kScale);
        canvas_b.emplace();
        renderer_b.emplace(*canvas_b);
    }


    app::Runtime runtime_a{};
    std::optional<app::Runtime> runtime_b;

    debug::DebugScope debug_scope;

    auto t0 = std::chrono::steady_clock::now();
    struct ClockCtx {
        std::chrono::steady_clock::time_point t0;
    } clock_ctx{t0};

    auto clock_now = [](void* ctx) noexcept -> std::uint32_t {
        auto* c = static_cast<ClockCtx*>(ctx);
        const auto now = std::chrono::steady_clock::now();
        return (std::uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - c->t0).count();
    };

    const gui::perf::TickSource tick = gui::perf::make_tick_source(clock_now, &clock_ctx);
    runtime_a.init(tick, !kEnableSecond);
    runtime_a.state.data.lamp_on = true;
    runtime_a.state.data.battery = 100;
    if (kEnableSecond) {
        runtime_b.emplace();
        runtime_b->init(tick, false);
        runtime_b->state.data.lamp_on = true;
        runtime_b->state.data.battery = 100;
    }
    debug_scope.init();

    bool quit_requested = false;
    struct WatchCtx {
        bool* quit{nullptr};
        debug::DebugScope* scope{nullptr};
    } watch{&quit_requested, &debug_scope};

    auto quit_watch = [](void* userdata, SDL_Event* e) -> bool {
        auto* ctx = static_cast<WatchCtx*>(userdata);
        if (!ctx) return true;
        auto* quit = ctx->quit;
        auto* scope = ctx->scope;
        if (!quit) return true;
        if (e->type == SDL_EVENT_QUIT) {
            *quit = true;
            return true;
        }
        if (e->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
            *quit = true;
            return true;
        }
        if (e->type == SDL_EVENT_KEY_DOWN && scope) {
            const SDL_Keycode key = e->key.key;
            float w = scope->get_omega();
            float z = scope->get_zeta();
            const bool fine = (e->key.mod & SDL_KMOD_SHIFT) != 0;
            const float w_step = fine ? 0.01f : 0.5f;
            const float z_step = fine ? 0.01f : 0.05f;
            bool changed = false;
            if (key == SDLK_LEFTBRACKET) { w -= w_step; changed = true; }
            else if (key == SDLK_RIGHTBRACKET) { w += w_step; changed = true; }
            else if (key == SDLK_MINUS) { z -= z_step; changed = true; }
            else if (key == SDLK_EQUALS) { z += z_step; changed = true; }
            if (changed) {
                scope->set_spring(w, z);
            }
        }
        return true;
    };
    SDL_AddEventWatch(quit_watch, &watch);

    std::uint32_t sim_ms = 0;
    std::uint32_t last_real_ms = 0;

    input::SDLRawSource raw_a(128, 64, kScale);
    raw_a.set_window_id(sdl_a.window_id());
    std::optional<input::SDLRawSource> raw_b;
    if (kEnableSecond && sdl_b) {
        raw_b.emplace(128, 64, kScale);
        raw_b->set_window_id(sdl_b->window_id());
    }

    // Input routing is handled by the runtime instances.

    auto simulate_battery = [&](app::Runtime& rt, std::uint32_t t_ms) {
        rt.simulate_battery(t_ms);
        if (rt.state.pages.current() == app::PageId::Main) {
            const float t = (float)t_ms * 0.0025f;
            for (int i = 0; i < 8; ++i) {
                const float phase = t + (float)i * 0.4f;
                const float s = (std::sin(phase) * 0.5f) + 0.5f;
                rt.state.data.chart[i] = (std::uint8_t)(s * 100.0f);
            }
        }
    };

    auto draw_state = [&](app::Runtime& runtime,
                          Canvas& canvas,
                          gui::Renderer<Canvas>& renderer,
                          backend::SDL3Backend<128, 64>& sdl,
                          const char* title) {
        app::draw_current_ui(renderer, runtime.state);

        if (runtime.state.ui.fps_overlay == gui::ui::Toggle::On) {
            char dbg_buf[32]{};
            std::snprintf(dbg_buf, sizeof(dbg_buf), "D:%d A:%d%s",
                          runtime.last_dirty.dirty_count,
                          runtime.last_dirty.dirty_area,
                          runtime.last_dirty.dirty_full ? "F" : "");
            renderer.drawText(2, 2, dbg_buf, true);
        }

        if (runtime.flush_canvas(
                canvas,
                [&]() -> bool {
                    sdl.update_texture(canvas, nullptr);
                    return true;
                },
                [&](const auto& dr) -> bool {
                    sdl.update_texture(canvas, &dr);
                    return true;
                })) {
            sdl.present_frame();
        }

        if (runtime.state.fps.update(runtime.state.tick)) {
            char title_buf[64]{};
            std::snprintf(title_buf, sizeof(title_buf), "%s  FPS: %.1f  D:%d A:%d%s",
                          title, runtime.state.fps.value(),
                          runtime.last_dirty.dirty_count,
                          runtime.last_dirty.dirty_area,
                          runtime.last_dirty.dirty_full ? " F" : "");
            sdl.set_title(title_buf);
        }
    };

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto real_ms = (std::uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        const std::uint32_t real_delta = real_ms - last_real_ms;
        last_real_ms = real_ms;
        sim_ms += (real_delta >> 0); // 1x speed
        runtime_a.tick_ui(sim_ms);
        if (kEnableSecond && runtime_b) {
            runtime_b->tick_ui(sim_ms);
        }

        raw_a.begin_frame(real_ms);
        if (kEnableSecond && raw_b) {
            raw_b->begin_frame(real_ms);
        }
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            raw_a.handle_event(e);
            if (kEnableSecond && raw_b) {
                raw_b->handle_event(e);
            }
        }

        runtime_a.pump_raw(raw_a, real_ms);
        if (kEnableSecond && raw_b && runtime_b) {
            runtime_b->pump_raw(*raw_b, real_ms);
        }

        if (quit_requested || raw_a.should_quit() ||
            (kEnableSecond && raw_b && raw_b->should_quit()) ||
            runtime_a.state.request_quit ||
            (kEnableSecond && runtime_b && runtime_b->state.request_quit)) {
            break;
        }

        runtime_a.pump_app(real_ms);
        if (kEnableSecond && runtime_b) {
            runtime_b->pump_app(real_ms);
        }

        simulate_battery(runtime_a, sim_ms);
        if (kEnableSecond && runtime_b) {
            simulate_battery(*runtime_b, sim_ms);
        }

        draw_state(runtime_a, canvas_a, renderer_a, sdl_a, kTitleA);
        if (kEnableSecond && sdl_b && runtime_b) {
            draw_state(*runtime_b, *canvas_b, *renderer_b, *sdl_b, kTitleB);
        }

        // --- Debug scope window (setpoint vs tracking) ---
        const auto& th = app::theme::current();
        const std::int16_t pad = (std::int16_t)th.list_pad;
        const gui::Rect list_area{
            pad,
            pad,
            (std::int16_t)(Canvas::kWidth - pad * 2),
            (std::int16_t)(Canvas::kHeight - pad * 2)
        };

        std::uint8_t setpoint = 0;
        std::uint8_t tracking = 0;
        if (runtime_a.state.pages.current() == app::PageId::Main) {
            const int count = app::MainPageState::item_count;
            const int idx = (runtime_a.state.semantics.focus.index < 0) ? 0 : runtime_a.state.semantics.focus.index;
            if (count > 1) {
                setpoint = (std::uint8_t)(idx * 100 / (count - 1));
            }

            const int y = (int)runtime_a.state.main_page.highlight_spring.value;
            const int item_h = (int)th.list_item_h;
            const int gap = (int)th.list_gap;
            const int stride = (item_h + gap > 0) ? (item_h + gap) : 1;
            int rel = y - list_area.y + item_h / 2;
            if (rel < 0) rel = 0;
            const int global_y = rel + (int)runtime_a.state.main_page.viewport.scroll_y;
            const int pos_q8 = (global_y * 256) / stride;
            int pos = pos_q8 / 256;
            if (pos < 0) pos = 0;
            if (pos > count - 1) pos = count - 1;
            if (count > 1) {
                tracking = (std::uint8_t)(pos * 100 / (count - 1));
            } else {
                tracking = 0;
            }
        }

        debug_scope.push(setpoint, tracking, real_delta);
        debug_scope.draw();
        runtime_a.state.ui.anim.spring_override = true;
        runtime_a.state.ui.anim.spring_preset = gui::motion::SpringPreset::Custom;
        runtime_a.state.ui.anim.spring_omega = debug_scope.get_omega();
        runtime_a.state.ui.anim.spring_zeta = debug_scope.get_zeta();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    return 0;
} catch (const std::exception& e) {
    // 这里尽量不引入 iostream，避免对嵌入式思维污染；PC端你可改成日志
    SDL_Log("Fatal: %s", e.what());
    std::fprintf(stderr, "Fatal: %s\n", e.what() ? e.what() : "(null)");
    std::fprintf(stderr, "SDL_GetError: %s\n", SDL_GetError());
    return 1;
}

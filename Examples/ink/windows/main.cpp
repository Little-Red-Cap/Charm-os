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


    app::AppState state_a{};
    app::AppState state_b{};
    state_a.init();
    state_a.data.lamp_on = true;
    state_a.data.battery = 100;
    if (kEnableSecond) {
        state_b.init();
        state_b.data.lamp_on = true;
        state_b.data.battery = 100;
    }

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
    app::set_tick_source(state_a, tick);
    if (kEnableSecond) {
        app::set_tick_source(state_b, tick);
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

    ::input::Router router{};
    gui::input::RawSampler raw_sampler_a{};
    std::optional<gui::input::RawSampler> raw_sampler_b;
    gui::ui::RouterIntentQueue<> router_queue_a{};
    std::optional<gui::ui::RouterIntentQueue<>> router_queue_b;
    router_queue_a.set_consume(!kEnableSecond);
    (void)router_queue_a.start(router);
    const auto router_policy_a = router_queue_a.policy();

    state_a.input_policies.set(gui::ui::InputPolicyId::Default, router_policy_a);
    state_a.input_policies.set(gui::ui::InputPolicyId::Encoder, router_policy_a);
    gui::ui::PolicyChain<2> policy_chain_a{};
    policy_chain_a.add(router_policy_a);
    state_a.input_policies.set(gui::ui::InputPolicyId::Custom, gui::ui::make_policy_chain(policy_chain_a));
    state_a.input_policy_id = gui::ui::InputPolicyId::Default;
    state_a.input_policy = state_a.input_policies.get(state_a.input_policy_id);

    if (kEnableSecond && raw_b) {
        raw_sampler_b.emplace();
        router_queue_b.emplace();
        router_queue_b->set_consume(false);
        (void)router_queue_b->start(router);
        const auto router_policy_b = router_queue_b->policy();
        state_b.input_policies.set(gui::ui::InputPolicyId::Default, router_policy_b);
        state_b.input_policies.set(gui::ui::InputPolicyId::Encoder, router_policy_b);
        gui::ui::PolicyChain<2> policy_chain_b{};
        policy_chain_b.add(router_policy_b);
        state_b.input_policies.set(gui::ui::InputPolicyId::Custom, gui::ui::make_policy_chain(policy_chain_b));
        state_b.input_policy_id = gui::ui::InputPolicyId::Default;
        state_b.input_policy = state_b.input_policies.get(state_b.input_policy_id);
    }

    int last_dirty_count_a = 0;
    int last_dirty_area_a = 0;
    bool last_dirty_full_a = false;
    int last_dirty_count_b = 0;
    int last_dirty_area_b = 0;
    bool last_dirty_full_b = false;

    auto simulate_battery = [&](app::AppState& st, std::uint32_t t_ms) {
        if (st.pages.current() == app::PageId::Main) {
            const std::uint32_t period = 6000;
            const std::uint32_t m = t_ms % period;
            int b = (m < period / 2) ? (100 - (int)(m * 100 / (period/2)))
                                    : (int)((m - period/2) * 100 / (period/2));
            if (b < 0) b = 0;
            if (b > 100) b = 100;
            st.data.battery = b;
            st.data.progress_demo = (std::uint8_t)b;
            const float t = (float)t_ms * 0.0025f;
            for (int i = 0; i < 8; ++i) {
                const float phase = t + (float)i * 0.4f;
                const float s = (std::sin(phase) * 0.5f) + 0.5f;
                st.data.chart[i] = (std::uint8_t)(s * 100.0f);
            }
        }
    };

    auto present_canvas = [&](Canvas& canvas,
                              backend::SDL3Backend<128, 64>& sdl,
                              int& last_dirty_count,
                              int& last_dirty_area,
                              bool& last_dirty_full) {
        if (canvas.dirty_count() <= 0) return;

        constexpr int kDirtyMaxRects = 4;
        constexpr int kDirtyAreaLimit = (Canvas::kWidth * Canvas::kHeight) / 2;
        const auto stats = canvas.dirty_stats();
        const bool too_many = (stats.count > kDirtyMaxRects);
        const bool too_big = (stats.area > kDirtyAreaLimit);
        const bool full = stats.full || too_many || too_big;

        last_dirty_count = stats.count;
        last_dirty_area = stats.area;
        last_dirty_full = full;

        if (full) {
            sdl.update_texture(canvas, nullptr);
        } else {
            const int n = canvas.dirty_count();
            for (int i = 0; i < n; ++i) {
                const auto dr = canvas.dirty_rect_at(i);
                sdl.update_texture(canvas, &dr);
            }
        }
        sdl.present_frame();
        canvas.clear_dirty();
    };

    auto draw_state = [&](Canvas& canvas,
                          gui::Renderer<Canvas>& renderer,
                          backend::SDL3Backend<128, 64>& sdl,
                          app::AppState& st,
                          int& last_dirty_count,
                          int& last_dirty_area,
                          bool& last_dirty_full,
                          const char* title) {
        app::draw_current_ui(renderer, st);

        if (st.ui.fps_overlay == gui::ui::Toggle::On) {
            char dbg_buf[32]{};
            std::snprintf(dbg_buf, sizeof(dbg_buf), "D:%d A:%d%s",
                          last_dirty_count, last_dirty_area, last_dirty_full ? "F" : "");
            renderer.drawText(2, 2, dbg_buf, true);
        }

        present_canvas(canvas, sdl, last_dirty_count, last_dirty_area, last_dirty_full);

        if (st.fps.update(st.tick)) {
            char title_buf[64]{};
            std::snprintf(title_buf, sizeof(title_buf), "%s  FPS: %.1f  D:%d A:%d%s",
                          title, st.fps.value(), last_dirty_count, last_dirty_area,
                          last_dirty_full ? " F" : "");
            sdl.set_title(title_buf);
        }
    };

    auto pump_raw = [&](input::SDLRawSource& raw,
                        gui::input::RawSampler& sampler,
                        std::uint32_t now_ms) {
        while (auto ev = sampler.poll(raw, now_ms)) {
            router.dispatch(*ev);
        }
    };

    while (true) {
        const auto now = std::chrono::steady_clock::now();
        const auto real_ms = (std::uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        const std::uint32_t real_delta = real_ms - last_real_ms;
        last_real_ms = real_ms;
        sim_ms += (real_delta >> 0); // 1x speed
        state_a.now_ms = sim_ms;
        if (kEnableSecond) {
            state_b.now_ms = sim_ms;
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

        pump_raw(raw_a, raw_sampler_a, real_ms);
        if (kEnableSecond && raw_b && raw_sampler_b) {
            pump_raw(*raw_b, *raw_sampler_b, real_ms);
        }

        if (quit_requested || raw_a.should_quit() ||
            (kEnableSecond && raw_b && raw_b->should_quit()) ||
            state_a.request_quit || (kEnableSecond && state_b.request_quit)) {
            break;
        }

        state_a.fps_ui.update(state_a.tick);
        if (kEnableSecond) {
            state_b.fps_ui.update(state_b.tick);
        }

        app::pump_input(state_a, real_ms);
        if (kEnableSecond) {
            app::pump_input(state_b, real_ms);
        }

        simulate_battery(state_a, sim_ms);
        if (kEnableSecond) {
            simulate_battery(state_b, sim_ms);
        }

        draw_state(canvas_a, renderer_a, sdl_a, state_a,
                   last_dirty_count_a, last_dirty_area_a, last_dirty_full_a, kTitleA);
        if (kEnableSecond && sdl_b) {
            draw_state(*canvas_b, *renderer_b, *sdl_b, state_b,
                       last_dirty_count_b, last_dirty_area_b, last_dirty_full_b, kTitleB);
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
        if (state_a.pages.current() == app::PageId::Main) {
            const int count = app::MainPageState::item_count;
            const int idx = (state_a.semantics.focus.index < 0) ? 0 : state_a.semantics.focus.index;
            if (count > 1) {
                setpoint = (std::uint8_t)(idx * 100 / (count - 1));
            }

            const int y = (int)state_a.main_page.highlight_spring.value;
            const int item_h = (int)th.list_item_h;
            const int gap = (int)th.list_gap;
            const int stride = (item_h + gap > 0) ? (item_h + gap) : 1;
            int rel = y - list_area.y + item_h / 2;
            if (rel < 0) rel = 0;
            const int global_y = rel + (int)state_a.main_page.viewport.scroll_y;
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
        state_a.ui.anim.spring_override = true;
        state_a.ui.anim.spring_preset = gui::motion::SpringPreset::Custom;
        state_a.ui.anim.spring_omega = debug_scope.get_omega();
        state_a.ui.anim.spring_zeta = debug_scope.get_zeta();

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

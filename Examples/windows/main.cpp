#include <chrono>
#include <cmath>
#include <thread>

#include "SDL3/SDL_error.h"
#include "SDL3/SDL_log.h"

import ui_color;
import ui_pixel_format;
import ui_framebuffer;
import ui_canvas;
import ui_render;

import ui_event;
import ui_layout;
import ui_object;
import ui_button;
import ui_text;
import ui_style;

import ui_gui;
import ui_checkbox;

// import gui.canvas_1bpp;
// import gui.renderer;
//
// import app.state;
// import app.ui;
// import app.logic;
// import app.logic_intent;
//
// import input.events;
// import input.sampler;
// import input.intent;
import input.source.sdl;

import backend.sdl3;

int main() try {
    using Canvas = gui::Canvas1bpp<128, 64>;
    Canvas canvas;
    gui::Renderer<Canvas> renderer(canvas);

    constexpr int kScale = 8;
    backend::SDL3Backend<128, 64> sdl("Monochrome GUI (Embedded-first)", kScale);

    app::AppState state{};
    state.init();
    state.data.lamp_on = true;
    state.data.battery = 100;

    auto t0 = std::chrono::steady_clock::now();
    std::uint32_t sim_ms = 0;
    std::uint32_t last_real_ms = 0;

    input::SDLRawSource raw(128, 64, kScale);
    input::Sampler sampler; // 可传配置：SamplerCfg{...}


    while (true) {
        // 每 ~16ms 一帧
        const auto now = std::chrono::steady_clock::now();
        const auto real_ms = (std::uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count();
        const std::uint32_t real_delta = real_ms - last_real_ms;
        last_real_ms = real_ms;
        // sim_ms += (real_delta >> 2); // 0.25x speed
        // sim_ms += (real_delta >> 1); // 0.5x speed
        sim_ms += (real_delta >> 0); // 1x speed
        state.now_ms = sim_ms;

        // 1) 采样（唯一事件泵：SDL_PollEvent 只在这里发生）
        raw.update(real_ms);

        // 2) 退出条件（窗口关闭 or 根页面 Back/Esc 请求退出）
        if (raw.should_quit()|| state.request_quit) break;

        // 3) 消费意图
        while (auto it = sampler.poll(raw, real_ms)) {
            app::apply_intent(state, it);
            if (state.request_quit) break;
        }

        // --- 画 UI ---
        app::draw_current_ui(renderer, state);

        // --- 显示 ---
        sdl.present(canvas);
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

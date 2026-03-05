module;

#include <array>
#include <cstdint>
#include <span>

#include "stm32h7xx_hal.h"

export module player.stm32h7.ink_demo;

import bsp.st7305;
import gui.canvas_1bpp;
import gui.renderer;
import gui.theme;
import out.api;
import out.channel;
import player.hqzy.app_state;
import player.hqzy.controller;
import player.hqzy.ui_ink;
import player.stm32h7.display_st7305;
import util.core;

namespace {
    static out::channel_sink* g_sink = nullptr;
    constexpr util::u32 kLogRetryMs = 20;
    constexpr uint16_t kKey0Pin = GPIO_PIN_8;
    constexpr uint16_t kWkup2Pin = GPIO_PIN_2;

    template <out::fixed_string Fmt, typename... Args>
    inline void log(Args&&... args) noexcept {
        if (!g_sink) return;
        const util::u32 start = HAL_GetTick();
        while (true) {
            auto r = out::try_println<Fmt>(*g_sink, std::forward<Args>(args)...);
            if (r) break;
            if (r.error() != out::errc::would_block) break;
            if ((HAL_GetTick() - start) > kLogRetryMs) break;
            HAL_Delay(1);
        }
        const util::u32 flush_start = HAL_GetTick();
        while (true) {
            auto r = g_sink->flush();
            if (r) break;
            if (r.error() != out::errc::would_block) break;
            if ((HAL_GetTick() - flush_start) > kLogRetryMs) break;
            HAL_Delay(1);
        }
    }

    constexpr auto kPanelGeom = bsp::st7305::kDefaultGeometry;
    constexpr int kPanelWidth = kPanelGeom.width;
    constexpr int kPanelHeight = kPanelGeom.height;
    constexpr std::size_t kPanelNativeSize = bsp::st7305::native_size_for(kPanelGeom);
    constexpr std::size_t kPanelLinearSize = bsp::st7305::linear_size_for(kPanelGeom);

    using Canvas = gui::Canvas1bpp<kPanelWidth, kPanelHeight>;
    using Renderer = gui::Renderer<Canvas>;

    static Canvas g_canvas{};
    static std::array<util::u8, kPanelNativeSize> g_native{};

    inline bool read_key0() noexcept {
        return HAL_GPIO_ReadPin(GPIOA, kKey0Pin) == GPIO_PIN_SET;
    }

    inline bool read_wkup2() noexcept {
        return HAL_GPIO_ReadPin(GPIOA, kWkup2Pin) == GPIO_PIN_SET;
    }
}

export void ink_set_console_sink(out::channel_sink& sink) noexcept {
    g_sink = &sink;
}

export bool ink_demo_render_once() noexcept {
    static_assert(Canvas::kBufSize == kPanelLinearSize,
                  "Canvas and ST7305 linear buffer size mismatch");
    gui::theme::set_current(true);
    Renderer renderer(g_canvas);
    player::hqzy::AppState state{};
    player::hqzy::init(state);
    renderer.clear(false);
    player::hqzy::render_ui(renderer, state);
    display_st7305_panel().pack_from_linear_1bpp(g_canvas.bytes(), g_native);
    const auto st = display_st7305_panel().flush_native(g_native);
    if (st != bsp::st7305::Status::ok) {
        log<"ink: flush failed">();
        return false;
    }
    log<"ink: render done">();
    return true;
}

export void ink_demo_run() noexcept {
    gui::theme::set_current(true);
    Renderer renderer(g_canvas);
    player::hqzy::AppState state{};
    player::hqzy::Controller controller{};
    player::hqzy::init(state);
    controller.attach(state);
    controller.scan_files();
    util::u32 last_frame = 0;
    util::u32 fps_last = HAL_GetTick();
    util::u32 fps_frames = 0;

    log<"ink: demo run (WKUP2: next page, KEY0: play/pause)">();

    while (true) {
        const util::u32 now = HAL_GetTick();
        const bool key0 = read_key0();
        const bool wkup2 = read_wkup2();
        controller.on_keys(key0, wkup2);
        controller.tick(now);

        if ((now - last_frame) < player::hqzy::kFrameMs) {
            HAL_Delay(2);
            continue;
        }
        last_frame = now;

        renderer.clear(false);
        player::hqzy::render_ui(renderer, state);

        display_st7305_panel().pack_from_linear_1bpp(g_canvas.bytes(), g_native);
        const auto st = display_st7305_panel().flush_native(g_native);
        if (st != bsp::st7305::Status::ok) {
            log<"ink: flush failed">();
            break;
        }
        fps_frames += 1;
        if ((now - fps_last) >= 1000u) {
            const util::u32 elapsed = now - fps_last;
            const util::u32 fps = (fps_frames * 1000u) / (elapsed ? elapsed : 1u);
            log<"ink: fps={}">(fps);
            fps_last = now;
            fps_frames = 0;
        }
    }
}

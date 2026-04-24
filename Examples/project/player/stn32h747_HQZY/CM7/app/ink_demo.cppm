module;

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include "stm32h7xx_hal.h"
#include "tim.h"

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
import player.stm32h7.audio_mp3_demo;
import player.stm32h7.display_st7305;
import util.core;

namespace {
    static out::channel_sink* g_sink = nullptr;
    constexpr util::u32 kLogRetryMs = 20;
    constexpr uint16_t kKey0Pin = GPIO_PIN_8;
    constexpr uint16_t kWkup2Pin = GPIO_PIN_2;
    constexpr uint16_t kEncKeyPin = GPIO_PIN_8;
    constexpr int kEncStep = 4;
    constexpr bool kEncKeyActiveHigh = false;

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

    inline bool read_encoder_key() noexcept {
        const bool raw = (HAL_GPIO_ReadPin(GPIOI, kEncKeyPin) == GPIO_PIN_SET);
        return kEncKeyActiveHigh ? raw : !raw;
    }

    struct EncoderState {
        std::uint16_t last{0};
        int accum{0};
        bool ok{false};
    };

    inline void encoder_init(EncoderState& st) noexcept {
        MX_TIM8_Init();
        if (HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL) != HAL_OK) {
            st.ok = false;
            return;
        }
        st.last = static_cast<std::uint16_t>(__HAL_TIM_GET_COUNTER(&htim8));
        st.accum = 0;
        st.ok = true;
    }

    inline int encoder_steps(EncoderState& st) noexcept {
        if (!st.ok) return 0;
        const std::uint16_t now = static_cast<std::uint16_t>(__HAL_TIM_GET_COUNTER(&htim8));
        const int delta = static_cast<std::int16_t>(now - st.last);
        st.last = now;
        if (delta == 0) return 0;
        st.accum += delta;
        int steps = 0;
        while (st.accum >= kEncStep) {
            st.accum -= kEncStep;
            ++steps;
        }
        while (st.accum <= -kEncStep) {
            st.accum += kEncStep;
            --steps;
        }
        return steps;
    }

    void display_yield() noexcept {
        if (audio_mp3_is_active()) {
            (void)audio_mp3_update();
        }
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
    display_st7305_set_yield(&display_yield);
    Renderer renderer(g_canvas);
    player::hqzy::AppState state{};
    player::hqzy::Controller controller{};
    player::hqzy::init(state);
    controller.attach(state);
    controller.scan_files();
    EncoderState encoder{};
    encoder_init(encoder);
    util::u32 last_frame = 0;
    util::u32 fps_last = HAL_GetTick();
    util::u32 fps_frames = 0;
    util::u32 last_render_ms = 0;
    util::u32 last_flush_ms = 0;
    util::u32 last_total_ms = 0;

    log<"ink: demo run (ENC: move/vol, ENC key: enter, KEY0: play, WKUP2: page)">();

    while (true) {
        const util::u32 now = HAL_GetTick();
        const bool key0 = read_key0();
        const bool wkup2 = read_wkup2();
        const bool enc_key = read_encoder_key();
        const int enc_steps = encoder_steps(encoder);
        controller.on_keys(key0, wkup2);
        controller.on_encoder(enc_steps, enc_key);
        if (state.stop_request) {
            state.stop_request = false;
            audio_mp3_stop();
            state.playing = false;
            state.paused = true;
            state.progress = 0;
            state.buffer = 0;
        }
        if (state.play_request && state.play_path[0] != '\0') {
            state.play_request = false;
            log<"ink: play {}">(state.play_path);
            const bool ok = audio_mp3_start(std::string_view{state.play_path});
            state.audio_ready = ok;
            state.playing = ok;
            state.paused = !ok;
            state.progress = 0;
            state.buffer = 0;
        }
        if (audio_mp3_is_active()) {
            (void)audio_mp3_update();
        } else if (state.playing) {
            state.playing = false;
            state.paused = true;
        }

        if (state.playing && audio_mp3_is_active()) {
            HAL_Delay(2);
            continue;
        }

        if ((now - last_frame) < player::hqzy::kFrameMs) {
            if (audio_mp3_is_active()) {
                (void)audio_mp3_update();
            }
            HAL_Delay(1);
            continue;
        }
        last_frame = now;
        controller.tick(now);

        const util::u32 frame_start = HAL_GetTick();
        renderer.clear(false);
        player::hqzy::render_ui(renderer, state);
        display_st7305_panel().pack_from_linear_1bpp(g_canvas.bytes(), g_native);
        last_render_ms = HAL_GetTick() - frame_start;
        const util::u32 flush_start = HAL_GetTick();
        const auto st = display_st7305_panel().flush_native(g_native);
        if (st != bsp::st7305::Status::ok) {
            log<"ink: flush failed">();
            break;
        }
        last_flush_ms = HAL_GetTick() - flush_start;
        last_total_ms = HAL_GetTick() - frame_start;
        fps_frames += 1;
        if ((now - fps_last) >= 1000u) {
            const util::u32 elapsed = now - fps_last;
            const util::u32 fps = (fps_frames * 1000u) / (elapsed ? elapsed : 1u);
            log<"ink: fps={} render={}ms flush={}ms total={}ms">(
                fps, last_render_ms, last_flush_ms, last_total_ms);
            fps_last = now;
            fps_frames = 0;
        }
    }
}

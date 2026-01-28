//
// Created by Joho on 2025/12/30.
//

module;
#include <optional>
#include <SDL3/SDL.h>

export module input.source.sdl;

import input.raw;
import input.intent;

export namespace input {

    struct SDLRawSource {
        const bool* ks{nullptr};
        int nkeys{0};

        int logical_w{0};
        int logical_h{0};
        int win_w{0};
        int win_h{0};

        // AB 相位队列（固定容量，零动态）---
        static constexpr int QN = 256;  // 足够大：每 detent 5 相位，256 可缓存 ~50 detents
        std::uint8_t q_[QN]{};
        int qh_{0}, qt_{0}, qc_{0};

        bool quit_{false};

        // 调试计数器
        int wheel_event_count_{0};
        int total_phases_pushed_{0};

        bool key_down_[4]{false,false,false,false};
        bool use_event_keys_{false};

        SDLRawSource(int lw = 0, int lh = 0, int initial_scale = 1) noexcept
            : logical_w(lw),
              logical_h(lh),
              win_w(lw * ((initial_scale > 0) ? initial_scale : 1)),
              win_h(lh * ((initial_scale > 0) ? initial_scale : 1)) {}

        // push 一个 AB 状态（0..3）到队列
        bool push_ab(std::uint8_t ab) noexcept {
            if (qc_ >= QN) {
                return false;
            }
            q_[qt_] = (ab & 0x03);
            qt_ = (qt_ + 1) % QN;
            ++qc_;
            total_phases_pushed_++;

            return true;
        }

        // pop 一个 AB 状态；空则返回 nullopt
        std::optional<std::uint8_t> pop_encoder_ab() noexcept {
            if (qc_ == 0) return std::nullopt;
            const std::uint8_t v = q_[qh_];
            qh_ = (qh_ + 1) % QN;
            --qc_;

            return v;
        }

#if 1
        // 把一个 detent 的相位序列入队（相位逐个喂 decoder）
        void enqueue_detent(bool cw) noexcept {
            if (cw) {
                // 00 -> 01 -> 11 -> 10 -> 00
                push_ab(0b00);
                push_ab(0b01);
                push_ab(0b11);
                push_ab(0b10);
                push_ab(0b00);
            } else {
                // 00 -> 10 -> 11 -> 01 -> 00
                push_ab(0b00);
                push_ab(0b10);
                push_ab(0b11);
                push_ab(0b01);
                push_ab(0b00);
            }
        }
#else
        std::uint8_t last_ab_{0};  // 记录最后一个相位,保证连续性
        void enqueue_detent(bool cw) noexcept {
            std::uint8_t seq[4];

            if (cw) {
                // 顺时针: 当前状态 -> +1 -> +1 -> +1 -> +1 (回到起点)
                switch (last_ab_) {
                case 0: seq[0]=1; seq[1]=3; seq[2]=2; seq[3]=0; break; // 00->01->11->10->00
                case 1: seq[0]=3; seq[1]=2; seq[2]=0; seq[3]=1; break; // 01->11->10->00->01
                case 2: seq[0]=0; seq[1]=1; seq[2]=3; seq[3]=2; break; // 10->00->01->11->10
                case 3: seq[0]=2; seq[1]=0; seq[2]=1; seq[3]=3; break; // 11->10->00->01->11
                }
            } else {
                // 逆时针: 当前状态 -> -1 -> -1 -> -1 -> -1
                switch (last_ab_) {
                case 0: seq[0]=2; seq[1]=3; seq[2]=1; seq[3]=0; break; // 00->10->11->01->00
                case 1: seq[0]=0; seq[1]=2; seq[2]=3; seq[3]=1; break; // 01->00->10->11->01
                case 2: seq[0]=3; seq[1]=1; seq[2]=0; seq[3]=2; break; // 10->11->01->00->10
                case 3: seq[0]=1; seq[1]=0; seq[2]=2; seq[3]=3; break; // 11->01->00->10->11
                }
            }

            for (int i = 0; i < 4; ++i) {
                if (!push_ab(seq[i])) break;
            }
            last_ab_ = seq[3]; // 更新最后状态

            SDL_Log("Enqueued detent %s, queue size: %d", cw ? "CW" : "CCW", qc_);
        }
#endif

        // 把滚轮的 y 值转成 detent 步数（触控板可能是小数）
        static int wheel_steps(float y) noexcept {
            if (y > 0.0f) {
                int s = (int)(y + 0.5f);
                return (s == 0) ? 1 : s;
            }
            if (y < 0.0f) {
                int s = (int)(y - 0.5f);
                return (s == 0) ? -1 : s;
            }
            return 0;
        }

        // --- RawSource 契约：每帧 update() 一次 ---
        void update(std::uint32_t now_ms) noexcept {
            SDL_PumpEvents();
            ks = SDL_GetKeyboardState(&nkeys);

            SDL_Event e;
            // 只在这里 PollEvent（建议 SDL backend 的 pump_quit 不再 PollEvent；
            // 如果还保留 pump_quit()，就让它不 PollEvent 或者改为读 quit_）。
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    quit_ = true;
                    continue;
                }

                if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                    win_w = (int)e.window.data1;
                    win_h = (int)e.window.data2;
                }

                if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) {
                    const bool down = (e.type == SDL_EVENT_KEY_DOWN);
                    use_event_keys_ = true;
                    switch (e.key.scancode) {
                        case SDL_SCANCODE_UP:        key_down_[0] = down; break;
                        case SDL_SCANCODE_DOWN:      key_down_[1] = down; break;
                        case SDL_SCANCODE_RETURN:
                        case SDL_SCANCODE_KP_ENTER:  key_down_[2] = down; break;
                        case SDL_SCANCODE_ESCAPE:
                        case SDL_SCANCODE_BACKSPACE: key_down_[3] = down; break;
                        default: break;
                    }
                }

                if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                    wheel_event_count_++;
                    const int steps = wheel_steps(e.wheel.y);

                    if (steps == 0) {
                        continue;
                    }

                    const bool cw = (steps > 0);
                    const int n = (steps > 0) ? steps : -steps;

                    for (int i = 0; i < n; ++i) {
                        enqueue_detent(cw);
                    }
                }
            }
        }

        // --- Buttons ---
        bool is_down(Button b) const noexcept {
            const int idx = (b == Button::Up) ? 0 : (b == Button::Down) ? 1 : (b == Button::Enter) ? 2 : 3;
            if (!ks || nkeys <= 0) return false;
            auto down_sc = [&](int sc) -> bool { return (sc >= 0 && sc < nkeys) ? ks[sc] : false; };

            if (use_event_keys_) {
                if (key_down_[idx]) return true;
            }

            switch (b) {
                case Button::Up:    return down_sc(SDL_SCANCODE_UP);
                case Button::Down:  return down_sc(SDL_SCANCODE_DOWN);
                case Button::Enter: return down_sc(SDL_SCANCODE_RETURN) || down_sc(SDL_SCANCODE_KP_ENTER);
                case Button::Back:  return down_sc(SDL_SCANCODE_ESCAPE) || down_sc(SDL_SCANCODE_BACKSPACE);
            }
            return false;
        }

        // --- Pointer / Axis（MVP：无）---
        PointerRaw read_pointer() const noexcept {
            if (logical_w <= 0 || logical_h <= 0) {
                return PointerRaw{.down = false, .x = 0, .y = 0, .id = 0};
            }

            int ww = win_w;
            int wh = win_h;
            if (SDL_Window* w = SDL_GetMouseFocus()) {
                int wpx = 0, hpx = 0;
                if (SDL_GetWindowSizeInPixels(w, &wpx, &hpx)) {
                    ww = wpx;
                    wh = hpx;
                }
            }
            if (ww <= 0 || wh <= 0) {
                return PointerRaw{.down = false, .x = 0, .y = 0, .id = 0};
            }

            float mx = 0.0f, my = 0.0f;
            const SDL_MouseButtonFlags btn = SDL_GetMouseState(&mx, &my);
            const bool down = (btn & SDL_BUTTON_LMASK) != 0;

            const int scale_w = ww / logical_w;
            const int scale_h = wh / logical_h;
            const int scale = (scale_w < scale_h) ? scale_w : scale_h;
            const int s = (scale > 0) ? scale : 1;

            const int dst_w = logical_w * s;
            const int dst_h = logical_h * s;
            const int dst_x = (ww - dst_w) / 2;
            const int dst_y = (wh - dst_h) / 2;

            const int lx = (int)((mx - (float)dst_x) / (float)s);
            const int ly = (int)((my - (float)dst_y) / (float)s);

            if (lx < 0 || ly < 0 || lx >= logical_w || ly >= logical_h) {
                return PointerRaw{.down = false, .x = (std::int16_t)-1, .y = (std::int16_t)-1, .id = 0};
            }

            return PointerRaw{.down = down, .x = (std::int16_t)lx, .y = (std::int16_t)ly, .id = 0};
        }
        AxisRaw    read_axis() const noexcept { return AxisRaw{0, 0}; }

        // quit 状态（可选给 main/backend 用）
        bool should_quit() const noexcept { return quit_; }
    };

} // namespace input

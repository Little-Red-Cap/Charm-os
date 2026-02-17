// debug.scope.cppm
// SDL debug scope window for setpoint/tracking visualization.

module;
#include <SDL3/SDL.h>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

export module debug.scope;

import gui.motion;

export namespace debug {

struct DebugScope {
    static constexpr int kTraceCount = 512;

    SDL_Window* win{nullptr};
    SDL_Renderer* ren{nullptr};
    std::uint8_t trace_set[kTraceCount]{};
    std::uint8_t trace_track[kTraceCount]{};
    std::uint8_t trace_spring[kTraceCount]{};
    int trace_phase{0};
    gui::motion::Spring1D spring{};
    bool spring_valid{false};
    bool show_spring{true};
    float omega{10.0f};
    float zeta{0.7f};

    void set_spring(float w, float z) noexcept {
        if (w < 0.1f) w = 0.1f;
        if (w > 100.0f) w = 100.0f;
        if (z < 0.0f) z = 0.0f;
        if (z > 2.0f) z = 2.0f;
        omega = w;
        zeta = z;
        spring.set_params(omega, zeta);
    }

    float get_omega() const noexcept { return omega; }
    float get_zeta() const noexcept { return zeta; }

    void init() {
        if (win) return;
        win = SDL_CreateWindow("Charm-ink Scope", 800, 400, SDL_WINDOW_RESIZABLE);
        if (!win) throw std::runtime_error(SDL_GetError());
        ren = SDL_CreateRenderer(win, nullptr);
        if (!ren) throw std::runtime_error(SDL_GetError());
    }

    void shutdown() {
        if (ren) { SDL_DestroyRenderer(ren); ren = nullptr; }
        if (win) { SDL_DestroyWindow(win); win = nullptr; }
    }

    ~DebugScope() { shutdown(); }

    void push(std::uint8_t setpoint, std::uint8_t tracking, std::uint32_t dt_ms) noexcept {
        trace_set[trace_phase] = setpoint;
        trace_track[trace_phase] = tracking;
        if (!spring_valid) {
            spring.reset((float)setpoint);
            set_spring(omega, zeta);
            spring_valid = true;
        }
        spring.target = (float)setpoint;
        spring.step_ms(dt_ms);
        int sv = (int)(spring.value + 0.5f);
        if (sv < 0) sv = 0;
        if (sv > 100) sv = 100;
        trace_spring[trace_phase] = (std::uint8_t)sv;
        trace_phase = (trace_phase + 1) % kTraceCount;
    }

    void draw() noexcept {
        if (!win || !ren) return;
        int dbg_w = 0, dbg_h = 0;
        SDL_GetWindowSize(win, &dbg_w, &dbg_h);
        const int left = 16;
        const int top = 24;
        const int w = (dbg_w > 32) ? (dbg_w - 32) : dbg_w;
        const int h = (dbg_h > 32) ? (dbg_h - 32) : dbg_h;

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);

        SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
        SDL_FRect border_rect{(float)left, (float)top, (float)w, (float)h};
        SDL_RenderRect(ren, &border_rect);
        SDL_RenderLine(ren, left, top + h / 2, left + w, top + h / 2);
        SDL_RenderLine(ren, left + w / 2, top, left + w / 2, top + h);

        const int inner_w = (w > 2) ? (w - 2) : 1;
        const int inner_h = (h > 2) ? (h - 2) : 1;
        const int x0 = left + 1;
        const int y0 = top + 1;

        auto sample_y = [&](std::uint8_t v) noexcept {
            return y0 + (inner_h - 1) - (int)((inner_h - 1) * v / 100);
        };

        // tracking: solid line (green)
        SDL_SetRenderDrawColor(ren, 0, 220, 0, 255);
        int prev_x = x0;
        int prev_y = sample_y(trace_track[trace_phase]);
        for (int i = 1; i < inner_w; ++i) {
            const int idx = (trace_phase + (i * kTraceCount) / inner_w) % kTraceCount;
            const int x = x0 + i;
            const int y = sample_y(trace_track[idx]);
            SDL_RenderLine(ren, prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;
        }

        // spring sim: solid line (cyan)
        if (show_spring) {
            SDL_SetRenderDrawColor(ren, 0, 200, 200, 255);
            int prev_x3 = x0;
            int prev_y3 = sample_y(trace_spring[trace_phase]);
            for (int i = 1; i < inner_w; ++i) {
                const int idx = (trace_phase + (i * kTraceCount) / inner_w) % kTraceCount;
                const int x = x0 + i;
                const int y = sample_y(trace_spring[idx]);
                SDL_RenderLine(ren, prev_x3, prev_y3, x, y);
                prev_x3 = x;
                prev_y3 = y;
            }
        }

        // setpoint: step plot (yellow)
        SDL_SetRenderDrawColor(ren, 220, 220, 0, 255);
        int prev_val = trace_set[trace_phase];
        int prev_x2 = x0;
        int prev_y2 = sample_y((std::uint8_t)prev_val);
        for (int i = 1; i < inner_w; ++i) {
            const int idx = (trace_phase + (i * kTraceCount) / inner_w) % kTraceCount;
            const int val = trace_set[idx];
            const int x = x0 + i;
            const int y = sample_y((std::uint8_t)val);
            SDL_RenderLine(ren, prev_x2, prev_y2, x, prev_y2);
            if (y != prev_y2) {
                SDL_RenderLine(ren, x, prev_y2, x, y);
            }
            prev_x2 = x;
            prev_y2 = y;
            prev_val = val;
        }

        // Legend + values
        const int legend_y = top - 12;
        const int legend_text_y = top - 20;
        SDL_SetRenderDrawColor(ren, 220, 220, 0, 255);
        SDL_RenderLine(ren, left + 6, legend_y, left + 26, legend_y);
        if (show_spring) {
            SDL_SetRenderDrawColor(ren, 0, 200, 200, 255);
            SDL_RenderLine(ren, left + 86, legend_y, left + 106, legend_y);
        }
        SDL_SetRenderDrawColor(ren, 0, 220, 0, 255);
        SDL_RenderLine(ren, left + 186, legend_y, left + 206, legend_y);
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDebugText(ren, left + 28, legend_text_y, "Set");
        if (show_spring) SDL_RenderDebugText(ren, left + 108, legend_text_y, "Spring");
        SDL_RenderDebugText(ren, left + 208, legend_text_y, "Track");
        char val_buf[128]{};
        const auto sp = trace_set[(trace_phase + kTraceCount - 1) % kTraceCount];
        const auto tr = trace_track[(trace_phase + kTraceCount - 1) % kTraceCount];
        if (show_spring) {
            const auto sv = trace_spring[(trace_phase + kTraceCount - 1) % kTraceCount];
            std::snprintf(val_buf, sizeof(val_buf), "Set:%u  Spring:%u  Track:%u  w=%.2f z=%.2f",
                          (unsigned)sp, (unsigned)sv, (unsigned)tr, (double)omega, (double)zeta);
        } else {
            std::snprintf(val_buf, sizeof(val_buf), "Set:%u  Track:%u  w=%.2f z=%.2f",
                          (unsigned)sp, (unsigned)tr, (double)omega, (double)zeta);
        }
        SDL_RenderDebugText(ren, left + 300, legend_text_y, val_buf);

        SDL_RenderPresent(ren);
    }
};

} // namespace debug

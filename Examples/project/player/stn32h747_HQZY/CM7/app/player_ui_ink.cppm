module;

#include <cstdint>
#include <cstdio>

export module player.hqzy.ui_ink;

import gui.core;
import gui.renderer;
import gui.theme;
import gui.widgets;
import player.hqzy.app_state;

export namespace player::hqzy {
    namespace detail {
        template <typename Renderer>
        inline void draw_header(Renderer& r, int width, const char* title) noexcept {
            r.drawRect(gui::Rect{1, 1, static_cast<std::int16_t>(width - 2), 20}, true);
            r.drawText(6, 6, title, true);
        }

        template <typename Renderer>
        inline void draw_footer(Renderer& r, int width, int height,
                                const char* line1, const char* line2) noexcept {
            r.drawRect(gui::Rect{6, static_cast<std::int16_t>(height - 40),
                                 static_cast<std::int16_t>(width - 12), 30}, true);
            r.drawText(10, static_cast<std::int16_t>(height - 32), line1, true);
            r.drawText(10, static_cast<std::int16_t>(height - 18), line2, true);
        }
    }

    template <typename Renderer>
    void render_now_playing(Renderer& r, int w, int h, const AppState& s) noexcept {
        detail::draw_header(r, w, "Charm Player");
        gui::progress_row(r, gui::Rect{6, 28, static_cast<std::int16_t>(w - 12), 14},
                          "Track", s.progress, false);
        gui::progress_row(r, gui::Rect{6, 46, static_cast<std::int16_t>(w - 12), 14},
                          "Buffer", s.buffer, false);

        r.drawRect(gui::Rect{6, 68, static_cast<std::int16_t>(w - 12), 60}, true);
        r.drawText(10, 76, "Now Playing:", true);
        r.drawText(10, 90, s.track.title, true);
        r.drawText(10, 104, s.track.format, true);

        r.drawRect(gui::Rect{6, 138, static_cast<std::int16_t>(w - 12), 44}, true);
        r.drawText(10, 146, s.fs_ready ? "SDMMC1 OK" : "SDMMC1 ERR", true);
        r.drawText(10, 160, s.audio_ready ? "I2S1 OK" : "I2S1 ERR", true);

        detail::draw_footer(r, w, h, "WKUP2: Next Page", "KEY0: Play/Pause");
    }

    template <typename Renderer>
    void render_metrics(Renderer& r, int w, int h, const AppState& s) noexcept {
        detail::draw_header(r, w, "System Metrics");
        r.drawRect(gui::Rect{6, 28, static_cast<std::int16_t>(w - 12), 64}, true);
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "Uptime %lus", (unsigned long)(s.uptime_ms / 1000u));
        r.drawText(10, 36, buf, true);
        std::snprintf(buf, sizeof(buf), "Frames %lu", (unsigned long)s.frames);
        r.drawText(10, 50, buf, true);
        r.drawText(10, 64, "Ink 1bpp", true);

        r.drawRect(gui::Rect{6, 104, static_cast<std::int16_t>(w - 12), 40}, true);
        r.drawText(10, 112, "SPI5 Mode0", true);
        r.drawText(10, 126, "ST7305 OK", true);

        detail::draw_footer(r, w, h, "WKUP2: Next Page", "KEY0: Play/Pause");
    }

    template <typename Renderer>
    void render_controls(Renderer& r, int w, int h, const AppState& s) noexcept {
        detail::draw_header(r, w, "Playback");
        gui::progress_row(r, gui::Rect{6, 28, static_cast<std::int16_t>(w - 12), 14},
                          "Volume", s.volume, false);
        r.drawRect(gui::Rect{6, 52, static_cast<std::int16_t>(w - 12), 70}, true);
        r.drawText(10, 60, s.playing ? "State: Playing" : "State: Paused", true);
        r.drawText(10, 74, "Use KEY0 toggle", true);
        r.drawText(10, 88, "Use WKUP2 page", true);

        detail::draw_footer(r, w, h, "Audio chain OK", "Ink UI running");
    }

    template <typename Renderer>
    void render_files(Renderer& r, int w, int h, const AppState& s) noexcept {
        detail::draw_header(r, w, "File List");
        r.drawRect(gui::Rect{6, 28, static_cast<std::int16_t>(w - 12),
                             static_cast<std::int16_t>(h - 68)}, true);
        const int list_x = 10;
        const int list_y = 34;
        const int row_h = 14;
        const int visible = (h - 78) / row_h;
        int start = 0;
        if (s.entry_selected >= visible) {
            start = static_cast<int>(s.entry_selected) - visible + 1;
        }
        for (int i = 0; i < visible; ++i) {
            const int idx = start + i;
            if (idx >= static_cast<int>(s.entry_count)) break;
            const auto& e = s.entries[idx];
            const int y = list_y + i * row_h;
            const bool selected = (idx == s.entry_selected);
            if (selected) {
                r.fillRect(gui::Rect{list_x - 2, static_cast<std::int16_t>(y - 1),
                                     static_cast<std::int16_t>(w - 20), static_cast<std::int16_t>(row_h)}, true);
                r.drawText(list_x, y, e.name, false);
            } else {
                r.drawText(list_x, y, e.name, true);
            }
            if (e.is_dir) {
                r.drawText(w - 18, y, "/", selected ? false : true);
            }
        }
        if (!s.list_ready) {
            r.drawText(10, static_cast<std::int16_t>(h - 52),
                       s.list_error ? "List failed" : "Scanning...", true);
        } else if (s.entry_count == 0) {
            r.drawText(10, static_cast<std::int16_t>(h - 52), "Empty", true);
        }
        detail::draw_footer(r, w, h, "WKUP2: Next Page", "KEY0: Play/Pause");
    }

    template <typename Renderer>
    void render_ui(Renderer& r, const AppState& s) noexcept {
        const int w = Renderer::kWidth;
        const int h = Renderer::kHeight;
        switch (s.page) {
        case Page::metrics:
            render_metrics(r, w, h, s);
            break;
        case Page::controls:
            render_controls(r, w, h, s);
            break;
        case Page::files:
            render_files(r, w, h, s);
            break;
        case Page::now_playing:
        default:
            render_now_playing(r, w, h, s);
            break;
        }
    }
}

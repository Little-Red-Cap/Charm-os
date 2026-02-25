// gui.ui_perf.cppm
// UI helpers for perf overlays (FPS, etc).

module;
#include <cstdint>
#include <cstring>
#include <expected>
#include <string_view>

export module gui.ui_perf;

import gui.core;
import gui.layout;
import gui.font;
import gui.theme;
import out.core;
import out.format;

namespace {
    struct trunc_sink {
        char* buf{nullptr};
        std::size_t cap{0};
        std::size_t pos{0};

        out::result<std::size_t> write(out::bytes b) noexcept
        {
            if (!buf || cap == 0) return std::unexpected(out::errc::buffer_overflow);
            const std::size_t avail = (pos < cap) ? (cap - pos) : 0;
            const std::size_t n = (b.size() < avail) ? b.size() : avail;
            if (n > 0) {
                std::memcpy(buf + pos, b.data(), n);
                pos += n;
            }
            if (n < b.size()) return std::unexpected(out::errc::buffer_overflow);
            return out::ok(b.size());
        }
    };

    template <out::fixed_string Fmt, class... Args>
    inline std::string_view format_to(char* buf, std::size_t size, Args&&... args) noexcept
    {
        if (!buf || size == 0) return {};
        trunc_sink sink{buf, size - 1u, 0u};
        (void)out::vprint<Fmt>(sink, std::forward<Args>(args)...);
        const std::size_t n = sink.pos;
        buf[n] = '\0';
        return {buf, n};
    }
}

export namespace gui::ui
{
    struct FpsOverlayStyle {
        const gui::Font* font{nullptr};
        std::int16_t pad_x{0};
        const char* label{"FPS"};
        bool align_right{true};
    };

    [[nodiscard]] inline FpsOverlayStyle default_fps_style(const gui::theme::ThemeSpec& theme) noexcept
    {
        FpsOverlayStyle st{};
        st.font = theme.font_default;
        st.pad_x = (std::int16_t)theme.pad_xs;
        st.label = "FPS";
        st.align_right = true;
        return st;
    }

    template <class R>
    inline void draw_fps_overlay(R& r,
                                 const FpsOverlayStyle& st,
                                 double fps_value,
                                 double fps_aux = -1.0) noexcept
    {
        if (!st.font) return;
        char buf[24]{};
        const char* label = st.label ? st.label : "FPS";
        const int fps10 = (int)(fps_value * 10.0 + 0.5);
        const int fps_i = fps10 / 10;
        const int fps_f = fps10 % 10;
        std::string_view text;
        if (fps_aux < 0.0) {
            text = format_to<"{}:{}.{}">(buf, sizeof(buf), label, fps_i, fps_f);
        } else {
            const int aux10 = (int)(fps_aux * 10.0 + 0.5);
            const int aux_i = aux10 / 10;
            const int aux_f = aux10 % 10;
            text = format_to<"{}:{}.{} / {}.{}">(buf, sizeof(buf),
                                                label, fps_i, fps_f, aux_i, aux_f);
        }
        const int w = gui::layout::text_width(*st.font, text);
        const int base = gui::layout::baseline_from_top(*st.font, st.pad_x);
        int x = st.align_right ? (R::kWidth - w - st.pad_x) : st.pad_x;
        if (x < 0) x = 0;
        r.drawText(*st.font, (std::int16_t)x, base, text, true);
    }

    template <class R>
    inline void draw_fps_overlay_default(R& r,
                                         const gui::theme::ThemeSpec& theme,
                                         double fps_value,
                                         bool enabled,
                                         double fps_aux = -1.0) noexcept
    {
        if (!enabled) return;
        const auto st = default_fps_style(theme);
        draw_fps_overlay(r, st, fps_value, fps_aux);
    }
} // namespace gui::ui

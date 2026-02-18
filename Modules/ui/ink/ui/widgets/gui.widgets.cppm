// Reusable widget drawing helpers (no list state or viewport).
// Keeps rendering details in one place for UI pages and list views.

module;
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <expected>
#include <span>
#include <string_view>

export module gui.widgets;

import gui.theme;
import gui.core;
import gui.layout;
import gui.font;
import gui.chart_scope;
import gui.image_1bpp;
import out.core;
import out.format;
import alg_arc;
import alg_line;

namespace gui::detail
{
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


export namespace gui
{
    inline constexpr int kNoOverrideY = -32768;

    enum class FocusStyle : std::uint8_t {
        ReverseRoundRect = 0,
        ReverseRect = 1,
        HollowRoundRect = 2,
        HollowRect = 3,
        Underline = 4,
        Block = 5,
    };

    enum class PatternKind : std::uint8_t {
        Solid = 0,
        Hatch45 = 1,
        Hatch135 = 2,
        Cross = 3,
        Dots = 4,
        Noise25 = 5,
        Noise50 = 6,
    };

    struct RowParams {
        Rect rc{};
        const char* label{nullptr};
        bool focused{false};
        std::uint32_t now_ms{0};
        bool frame{false};
        int highlight_y{kNoOverrideY};
        const Rect* invert{nullptr};
    };

    struct ChartView {
        const std::uint8_t* data{nullptr};
        std::uint8_t        count{0};
    };

    struct ScopeView {
        const std::uint8_t* data_u8{nullptr};
        std::uint8_t        count_u8{0};
        const std::uint16_t* data_u16{nullptr};
        std::uint16_t        count_u16{0};
        std::uint16_t        min_range{0};
        bool                draw_grid{true};
        bool                draw_border{true};
    };

    struct SparklineView {
        const std::uint8_t* data{nullptr};
        std::uint8_t        count{0};
        std::uint8_t        min_v{0};
        std::uint8_t        max_v{100};
        bool               draw_border{false};
    };

    [[nodiscard]] inline ScopeView make_scope_view_u8(const std::uint8_t* data,
                                                      std::uint8_t count) noexcept
    {
        ScopeView v{};
        v.data_u8 = data;
        v.count_u8 = count;
        return v;
    }

    [[nodiscard]] inline ScopeView make_scope_view_u16(const std::uint16_t* data,
                                                       std::uint16_t count,
                                                       std::uint16_t min_range) noexcept
    {
        ScopeView v{};
        v.data_u16 = data;
        v.count_u16 = count;
        v.min_range = min_range;
        return v;
    }

    template <class R>
    void draw_line(R& r, int x0, int y0, int x1, int y1, bool on) noexcept
    {
        alg::line::raster(x0, y0, x1, y1, [&](int x, int y) noexcept {
            r.setPixel(x, y, on);
        });
    }

    template <class R>
    void draw_scope_grid(R& r, const Rect& rc) noexcept
    {
        r.drawRect(rc, true);

        const int x_mid = rc.x + rc.w / 2;
        const int y_mid = rc.y + rc.h / 2;
        for (int x = rc.x + 1; x < rc.x + rc.w - 1; ++x) {
            if ((x % 2) == 0) r.setPixel(x, y_mid, true);
        }
        for (int y = rc.y + 1; y < rc.y + rc.h - 1; ++y) {
            if ((y % 2) == 0) r.setPixel(x_mid, y, true);
        }

        const int x_q1 = rc.x + rc.w / 4;
        const int x_q3 = rc.x + (rc.w * 3) / 4;
        const int y_q1 = rc.y + rc.h / 4;
        const int y_q3 = rc.y + (rc.h * 3) / 4;
        for (int y = rc.y + 1; y < rc.y + rc.h - 1; ++y) {
            if ((y % 4) == 0) {
                r.setPixel(x_q1, y, true);
                r.setPixel(x_q3, y, true);
            }
        }
        for (int x = rc.x + 1; x < rc.x + rc.w - 1; ++x) {
            if ((x % 4) == 0) {
                r.setPixel(x, y_q1, true);
                r.setPixel(x, y_q3, true);
            }
        }
    }

    template <class R>
    void draw_scope_wave(R& r, const Rect& rc, const std::uint8_t* data,
                         int count, int phase) noexcept
    {
        if (!data || count <= 1 || rc.w <= 2 || rc.h <= 2) return;
        const int inner_w = rc.w - 2;
        const int inner_h = rc.h - 2;
        int prev_x = rc.x + 1;
        const int sample_index = (phase >= 0) ? (phase % count) : 0;
        const int prev_v = (int)data[sample_index];
        int prev_y = rc.y + 1 + (inner_h - 1) - (inner_h - 1) * prev_v / 100;
        for (int i = 1; i < inner_w; ++i) {
            const int idx = (phase + (i * count) / inner_w) % count;
            const int v = (int)data[idx];
            const int x = rc.x + 1 + i;
            const int y = rc.y + 1 + (inner_h - 1) - (inner_h - 1) * v / 100;
            draw_line(r, prev_x, prev_y, x, y, true);
            prev_x = x;
            prev_y = y;
        }
    }

    template <class R>
    void scope_widget(R& r, const Rect& rc, const ScopeView& view, int phase) noexcept
    {
        if (view.draw_border) {
            r.drawRect(rc, true);
        }
        if (view.draw_grid) {
            draw_scope_grid(r, rc);
        }
        if (view.data_u16 && view.count_u16 > 1) {
            const Rect inner{
                (std::int16_t)(rc.x + 1),
                (std::int16_t)(rc.y + 1),
                (std::int16_t)(rc.w - 2),
                (std::int16_t)(rc.h - 2)
            };
            gui::chart_scope::draw_wave([&](int x, int y) noexcept { r.setPixel(x, y, true); },
                                        inner.x,
                                        inner.y,
                                        inner.w,
                                        inner.h,
                                        std::span<const std::uint16_t>{view.data_u16, (std::size_t)view.count_u16},
                                        view.min_range);
            return;
        }
        draw_scope_wave(r, rc, view.data_u8, view.count_u8, phase);
    }

    template <class R>
    void scope_wave_widget(R& r,
                           const Rect& rc,
                           std::span<const std::uint16_t> wave,
                           std::uint16_t min_range,
                           bool border = true) noexcept
    {
        ScopeView v = make_scope_view_u16(wave.data(), (std::uint16_t)wave.size(), min_range);
        v.draw_grid = false;
        v.draw_border = border;
        scope_widget(r, rc, v, 0);
    }

    inline void fill_round_rect(auto& r, const Rect& rc) noexcept
    {
        if (rc.w <= 0 || rc.h <= 0) return;
        r.fillRect(rc, true);
        if (rc.w < 3 || rc.h < 3) return;
        r.setPixel(rc.x, rc.y, false);
        r.setPixel(rc.x + rc.w - 1, rc.y, false);
        r.setPixel(rc.x, rc.y + rc.h - 1, false);
        r.setPixel(rc.x + rc.w - 1, rc.y + rc.h - 1, false);
    }

    template <class R>
    void draw_round_rect(R& r, const Rect& rc, bool on) noexcept
    {
        if (rc.w <= 0 || rc.h <= 0) return;
        if (rc.w < 4 || rc.h < 4) {
            r.drawRect(rc, on);
            return;
        }
        const int x0 = rc.x;
        const int y0 = rc.y;
        const int x1 = rc.x + rc.w - 1;
        const int y1 = rc.y + rc.h - 1;
        for (int x = x0 + 2; x <= x1 - 2; ++x) {
            r.setPixel(x, y0, on);
            r.setPixel(x, y1, on);
        }
        for (int y = y0 + 2; y <= y1 - 2; ++y) {
            r.setPixel(x0, y, on);
            r.setPixel(x1, y, on);
        }
        r.setPixel(x0 + 1, y0 + 1, on);
        r.setPixel(x1 - 1, y0 + 1, on);
        r.setPixel(x0 + 1, y1 - 1, on);
        r.setPixel(x1 - 1, y1 - 1, on);
    }

    template <class R>
    void draw_arc(R& r, int cx, int cy, int rad, float a0, float a1, bool on) noexcept
    {
        const int steps = alg::arc::arc_steps_for_radius(rad);
        alg::arc::sample_arc_rad(a0, a1, steps, [&](float a) noexcept {
            const auto p = alg::arc::point_on_circle_rad(cx, cy, rad, a);
            r.setPixel(p.x, p.y, on);
        });
    }

    template <class R>
    void draw_circle(R& r, int cx, int cy, int rad, bool on) noexcept
    {
        int x = rad;
        int y = 0;
        int err = 0;
        while (x >= y) {
            r.setPixel(cx + x, cy + y, on);
            r.setPixel(cx + y, cy + x, on);
            r.setPixel(cx - y, cy + x, on);
            r.setPixel(cx - x, cy + y, on);
            r.setPixel(cx - x, cy - y, on);
            r.setPixel(cx - y, cy - x, on);
            r.setPixel(cx + y, cy - x, on);
            r.setPixel(cx + x, cy - y, on);
            if (err <= 0) {
                ++y;
                err += 2 * y + 1;
            }
            if (err > 0) {
                --x;
                err -= 2 * x + 1;
            }
        }
    }

    template <class R>
    void draw_gauge(R& r, const Rect& rc, std::uint8_t value_0_100, bool on = true) noexcept
    {
        if (rc.w <= 0 || rc.h <= 0) return;
        const int cx = rc.x + rc.w / 2;
        const int cy = rc.y + rc.h - 1;
        int rad = (rc.w / 2);
        if (rc.h < rad) rad = rc.h;
        if (rad < 4) rad = 4;
        const float a0 = alg::arc::kPi * 0.75f;
        const float a1 = alg::arc::kPi * 0.25f;
        draw_arc(r, cx, cy, rad, a0, a1, on);
        draw_arc(r, cx, cy, rad - 1, a0, a1, on);
        for (int i = 0; i <= 4; ++i) {
            const float t = (float)i / 4.0f;
            const float a = a0 + (a1 - a0) * t;
            const auto p0 = alg::arc::point_on_circle_rad(cx, cy, rad - 3, a);
            const auto p1 = alg::arc::point_on_circle_rad(cx, cy, rad, a);
            draw_line(r, p0.x, p0.y, p1.x, p1.y, on);
        }
        const float v = alg::arc::ratio_from_range(value_0_100, 0, 100);
        const float a = alg::arc::lerp(a0, a1, v);
        const auto needle = alg::arc::point_on_circle_rad(cx, cy, rad - 4, a);
        draw_line(r, cx, cy, needle.x, needle.y, on);
    }

    template <class R>
    void draw_sparkline(R& r, const Rect& rc, const SparklineView& v, bool on = true) noexcept
    {
        if (!v.data || v.count < 2 || rc.w <= 1 || rc.h <= 1) return;
        const int minv = v.min_v;
        const int maxv = (v.max_v > v.min_v) ? v.max_v : (v.min_v + 1);
        if (v.draw_border) {
            r.drawRect(rc, on);
        }
        const int w = rc.w;
        const int h = rc.h;
        int prev_x = rc.x;
        int prev_y = rc.y + h - 1 - (h - 1) * (v.data[0] - minv) / (maxv - minv);
        for (int i = 1; i < w; ++i) {
            const int idx = (i * (v.count - 1)) / (w - 1);
            const int val = v.data[idx];
            const int x = rc.x + i;
            const int y = rc.y + h - 1 - (h - 1) * (val - minv) / (maxv - minv);
            draw_line(r, prev_x, prev_y, x, y, on);
            prev_x = x;
            prev_y = y;
        }
    }

    template <class R>
    void draw_segment_digit(R& r, const Rect& rc, int digit, bool on = true) noexcept
    {
        if (rc.w <= 2 || rc.h <= 4) return;
        const int t = (rc.h < 16) ? 2 : 3;
        const int x = rc.x;
        const int y = rc.y;
        const int w = rc.w;
        const int h = rc.h;
        const int mid = y + h / 2;
        auto seg = [&](int sx, int sy, int sw, int sh) noexcept {
            r.fillRect(Rect{(std::int16_t)sx, (std::int16_t)sy, (std::int16_t)sw, (std::int16_t)sh}, on);
        };
        const bool s0 = (digit != 1 && digit != 4);
        const bool s1 = (digit != 5 && digit != 6);
        const bool s2 = (digit != 2);
        const bool s3 = (digit != 1 && digit != 4 && digit != 7);
        const bool s4 = (digit == 0 || digit == 2 || digit == 6 || digit == 8);
        const bool s5 = (digit != 1 && digit != 2 && digit != 3 && digit != 7);
        const bool s6 = (digit != 0 && digit != 1 && digit != 7);
        if (s0) seg(x + t, y, w - 2 * t, t);
        if (s1) seg(x + w - t, y + t, t, mid - y - t);
        if (s2) seg(x + w - t, mid + t / 2, t, y + h - mid - t / 2 - t);
        if (s3) seg(x + t, y + h - t, w - 2 * t, t);
        if (s4) seg(x, mid + t / 2, t, y + h - mid - t / 2 - t);
        if (s5) seg(x, y + t, t, mid - y - t);
        if (s6) seg(x + t, mid - t / 2, w - 2 * t, t);
    }

    template <class R>
    void draw_icon_label_badge(R& r,
                               const Rect& rc,
                               const Image1bpp* icon,
                               const char* label,
                               const char* badge,
                               bool focused) noexcept
    {
        if (rc.w <= 0 || rc.h <= 0) return;
        const auto& th = gui::theme::current();
        if (!th.font_default) return;

        if (focused) {
            fill_round_rect(r, rc);
        } else {
            r.drawRect(rc, true);
        }

        const int text_on = focused ? 0 : 1;
        const int pad = th.pad_xs;
        const int icon_w = icon ? icon->width : 0;
        const int icon_h = icon ? icon->height : 0;
        const int icon_x = rc.x + pad;
        const int icon_y = rc.y + (rc.h - icon_h) / 2;
        if (icon && icon_w > 0 && icon_h > 0) {
            draw_image_1bpp(r, (std::int16_t)icon_x, (std::int16_t)icon_y, *icon, text_on != 0);
        }

        int label_x = rc.x + pad;
        if (icon_w > 0) {
            label_x += icon_w + pad;
        }
        const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
        r.drawText(*th.font_default, (std::int16_t)label_x, (std::int16_t)base,
                   label ? label : "", text_on != 0);

        if (badge && badge[0] != '\0') {
            const int badge_w = gui::measure_text(*th.font_default, badge) + 6;
            const int badge_h = th.font_default->line_height + 2;
            const int bx = rc.x + rc.w - pad - badge_w;
            const int by = rc.y + (rc.h - badge_h) / 2;
            const Rect br{(std::int16_t)bx, (std::int16_t)by, (std::int16_t)badge_w, (std::int16_t)badge_h};
            const bool fill_on = focused ? false : true;
            const bool text_on2 = focused ? true : false;
            if (fill_on) {
                fill_round_rect(r, br);
            } else {
                r.drawRect(br, true);
            }
            const int bbase = gui::layout::baseline_from_top(*th.font_default, br.y + 1);
            r.drawText(*th.font_default, (std::int16_t)(br.x + 3), (std::int16_t)bbase,
                       badge, text_on2);
        }
    }

    template <class R>
    void draw_tag(R& r, const Rect& rc, const Font& font, const char* text, bool on = true) noexcept
    {
        if (!text || rc.w <= 0 || rc.h <= 0) return;
        fill_round_rect(r, rc);
        const int base = gui::layout::baseline_from_top(font, rc.y + 2);
        r.drawText(font, (std::int16_t)(rc.x + 4), (std::int16_t)base, text, false);
        r.drawRect(rc, on);
    }

    template <class R>
    void draw_knob(R& r, const Rect& rc, std::uint8_t value_0_100, bool on = true) noexcept
    {
        if (rc.w <= 0 || rc.h <= 0) return;
        const int cx = rc.x + rc.w / 2;
        const int cy = rc.y + rc.h / 2;
        int rad = (rc.w < rc.h) ? (rc.w / 2) : (rc.h / 2);
        if (rad < 4) rad = 4;
        draw_circle(r, cx, cy, rad, on);
        draw_circle(r, cx, cy, rad - 1, on);
        const float a0 = alg::arc::kPi * 0.75f;
        const float a1 = alg::arc::kPi * 2.25f;
        for (int i = 0; i < 6; ++i) {
            const float t = (float)i / 5.0f;
            const float a = a0 + (a1 - a0) * t;
            const auto p0 = alg::arc::point_on_circle_rad(cx, cy, rad - 2, a);
            const auto p1 = alg::arc::point_on_circle_rad(cx, cy, rad, a);
            draw_line(r, p0.x, p0.y, p1.x, p1.y, on);
        }
        const float v = alg::arc::ratio_from_range(value_0_100, 0, 100);
        const float a = alg::arc::lerp(a0, a1, v);
        const auto needle = alg::arc::point_on_circle_rad(cx, cy, rad - 4, a);
        draw_line(r, cx, cy, needle.x, needle.y, on);
    }

    [[nodiscard]] inline std::int16_t marquee_offset(std::int16_t text_w,
                                                     std::int16_t view_w,
                                                     std::uint32_t now_ms,
                                                     std::uint16_t speed_px_s = 24,
                                                     std::uint16_t pause_ms = 500) noexcept
    {
        if (text_w <= view_w) return 0;
        const std::uint32_t span = (std::uint32_t)(text_w - view_w);
        const std::uint32_t travel_ms = (speed_px_s > 0) ? (span * 1000u / speed_px_s) : 1u;
        const std::uint32_t period = travel_ms + pause_ms * 2u;
        const std::uint32_t t = (period > 0) ? (now_ms % period) : 0u;
        if (t < pause_ms) return 0;
        if (t > pause_ms + travel_ms) return (std::int16_t)span;
        const std::uint32_t run = t - pause_ms;
        return (std::int16_t)((run * span) / travel_ms);
    }

    template <class R>
    void fill_pattern(R& r, const Rect& rc, PatternKind kind, bool on) noexcept
    {
        if (rc.w <= 0 || rc.h <= 0) return;
        if (kind == PatternKind::Solid) {
            r.fillRect(rc, on);
            return;
        }
        const int x0 = rc.x;
        const int y0 = rc.y;
        const int x1 = rc.x + rc.w;
        const int y1 = rc.y + rc.h;
        for (int y = y0; y < y1; ++y) {
            for (int x = x0; x < x1; ++x) {
                bool pix = false;
                switch (kind) {
                case PatternKind::Hatch45:
                    pix = ((x + y) & 0x3) == 0;
                    break;
                case PatternKind::Hatch135:
                    pix = ((x - y) & 0x3) == 0;
                    break;
                case PatternKind::Cross:
                    pix = ((x & 0x3) == 0) || ((y & 0x3) == 0);
                    break;
                case PatternKind::Dots:
                    pix = ((x & 0x3) == 0) && ((y & 0x3) == 0);
                    break;
                case PatternKind::Noise25:
                    pix = ((x * 17 + y * 31) & 0x7) == 0;
                    break;
                case PatternKind::Noise50:
                    pix = ((x * 13 + y * 29) & 0x3) == 0;
                    break;
                case PatternKind::Solid:
                default:
                    pix = true;
                    break;
                }
                if (pix) r.setPixel(x, y, on);
            }
        }
    }

    template <class R>
    void draw_focus_rect(R& r, const Rect& rc, FocusStyle style) noexcept
    {
        if (rc.w <= 0 || rc.h <= 0) return;
        switch (style) {
        case FocusStyle::ReverseRect:
            r.fillRect(rc, true);
            break;
        case FocusStyle::ReverseRoundRect:
            fill_round_rect(r, rc);
            break;
        case FocusStyle::HollowRect:
            r.drawRect(rc, true);
            break;
        case FocusStyle::HollowRoundRect:
            draw_round_rect(r, rc, true);
            break;
        case FocusStyle::Underline:
            r.fillRect(Rect{rc.x, (std::int16_t)(rc.y + rc.h - 1), rc.w, 1}, true);
            break;
        case FocusStyle::Block:
        default:
            {
                const std::int16_t s = (rc.h < 3) ? rc.h : 3;
                const std::int16_t y = (std::int16_t)(rc.y + (rc.h - s) / 2);
                r.fillRect(Rect{rc.x, y, s, s}, true);
            }
            break;
        }
    }

    inline void draw_label_bg(auto& r, const Rect& rc, const char* label, int pad_left,
                              int   y_override = kNoOverrideY) noexcept
    {
        const auto& th     = gui::theme::current();
        const char* text   = label ? label : "";
        const int   text_w = gui::layout::text_width(*th.font_default, text);
        const int   max_w  = rc.w - pad_left - 2;
        int         w      = text_w + 2;
        if (w > max_w) w = max_w;
        if (w <= 0) return;
        int x = rc.x + pad_left - 1;
        if (x < rc.x) x = rc.x;
        const int h = th.font_default->line_height + 2;
        const int y = (y_override != kNoOverrideY) ? y_override : (rc.y + (rc.h - h) / 2);
        fill_round_rect(r, Rect{(std::int16_t)x, (std::int16_t)y, (std::int16_t)w, (std::int16_t)h});
    }

    inline bool label_bg_rect_font(const Rect& rc, const Font& font, const char* label, int pad_left,
                                   int y_override, Rect& out) noexcept
    {
        const char* text   = label ? label : "";
        const int   text_w = gui::layout::text_width(font, text);
        const int   max_w  = rc.w - pad_left - 2;
        int         w      = text_w + 2;
        if (w > max_w) w = max_w;
        if (w <= 0) return false;
        int x = rc.x + pad_left - 1;
        if (x < rc.x) x = rc.x;
        const int h = font.line_height + 2;
        const int y = (y_override != kNoOverrideY) ? y_override : (rc.y + (rc.h - h) / 2);
        out = Rect{(std::int16_t)x, (std::int16_t)y, (std::int16_t)w, (std::int16_t)h};
        return true;
    }

    inline bool label_bg_rect_centered(const Rect& rc, const Font& font, const char* label,
                                       int y_override, Rect& out) noexcept
    {
        const char* text   = label ? label : "";
        const int   text_w = gui::layout::text_width(font, text);
        const int   max_w  = rc.w - 2;
        int         w      = text_w + 2;
        if (w > max_w) w = max_w;
        if (w <= 0) return false;
        int x = gui::layout::align_center_x(rc, text_w) - 1;
        if (x < rc.x) x = rc.x;
        if (x + w > rc.x + rc.w) x = rc.x + rc.w - w;
        const int h = font.line_height + 2;
        const int y = (y_override != kNoOverrideY) ? y_override : (rc.y + (rc.h - h) / 2);
        out = Rect{(std::int16_t)x, (std::int16_t)y, (std::int16_t)w, (std::int16_t)h};
        return true;
    }

    inline bool label_bg_rect(const Rect& rc, const char* label, int pad_left,
                              int y_override, Rect& out) noexcept
    {
        const auto& th = gui::theme::current();
        return label_bg_rect_font(rc, *th.font_default, label, pad_left, y_override, out);
    }

    inline void draw_label_scrolling(auto&         r, const Rect& rc, const char* label, bool on,
                                     std::uint32_t now_ms, int    pad_left, int   pad_top) noexcept
    {
        (void)pad_top;
        const auto& th     = gui::theme::current();
        const char* text   = label ? label : "";
        const int   base   = gui::layout::row_baseline_centered(*th.font_default, rc);
        const int   max_w  = rc.w - pad_left - 2;
        const int   text_w = gui::layout::text_width(*th.font_default, text);

        if (text_w <= max_w) {
            r.drawText(*th.font_default, rc.x + pad_left, base, text, on);
            return;
        }

        const int gap    = 8;
        const int period = text_w + gap;
        const int offset = (period > 0) ? (int)((now_ms / 80) % (std::uint32_t)period) : 0;
        const int x0     = rc.x + pad_left - offset;
        r.drawText(*th.font_default, x0, base, text, on);
        if (x0 + text_w < rc.x + pad_left + max_w) {
            r.drawText(*th.font_default, x0 + text_w + gap, base, text, on);
        }
    }

    inline void draw_label_scrolling_masked(auto& r, const Rect& rc, const char* label, std::uint32_t now_ms,
                                            int pad_left, int pad_top, const Rect* invert) noexcept
    {
        if (!invert) {
            draw_label_scrolling(r, rc, label, true, now_ms, pad_left, pad_top);
            return;
        }

        const auto& th = gui::theme::current();
        const char* text = label ? label : "";
        const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
        const int max_w = rc.w - pad_left - 2;
        const int text_w = gui::layout::text_width(*th.font_default, text);

        if (text_w <= max_w) {
            gui::draw_text_masked(r, *th.font_default, rc.x + pad_left, base, text, true, *invert);
            return;
        }

        const int gap = 8;
        const int period = text_w + gap;
        const int offset = (period > 0) ? (int)((now_ms / 80) % (std::uint32_t)period) : 0;
        const int x0 = rc.x + pad_left - offset;
        gui::draw_text_masked(r, *th.font_default, x0, base, text, true, *invert);
        if (x0 + text_w < rc.x + pad_left + max_w) {
            gui::draw_text_masked(r, *th.font_default, x0 + text_w + gap, base, text, true, *invert);
        }
    }

    template <class R>
    void selectable_row(R& r, const Rect& rc, const char* label, const bool focused) noexcept
    {
        const auto& th = gui::theme::current();
        Rect highlight{};
        const Rect* invert = nullptr;
        if (focused && label_bg_rect(rc, label, th.pad_xs, kNoOverrideY, highlight)) {
            fill_round_rect(r, highlight);
            invert = &highlight;
        }
        r.drawRect(rc, true);
        const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
        if (invert) {
            gui::draw_text_masked(r, *th.font_default, rc.x + th.pad_xs, base, label ? label : "", true, *invert);
        } else {
            r.drawText(*th.font_default, rc.x + th.pad_xs, base, label ? label : "", true);
        }
    }

    template <class R>
    // Legacy overload: prefer RowParams version.
    void selectable_row(R&         r, const Rect& rc, const char* label, const bool focused, std::uint32_t now_ms,
                        const bool frame, int     highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);
        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);
    }

    template <class R>
    inline void selectable_row(R& r, const RowParams& p) noexcept
    {
        selectable_row(r, p.rc, p.label, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    void toggle_row(R& r, const Rect& rc, const char* label, const bool value, const bool focused) noexcept
    {
        const auto& th = gui::theme::current();
        Rect highlight{};
        const Rect* invert = nullptr;
        if (focused && label_bg_rect(rc, label, th.pad_xs, kNoOverrideY, highlight)) {
            fill_round_rect(r, highlight);
            invert = &highlight;
        }
        r.drawRect(rc, true);

        const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
        if (invert) {
            gui::draw_text_masked(r, *th.font_default, rc.x + th.pad_xs, base, label ? label : "", true, *invert);
        } else {
            r.drawText(*th.font_default, rc.x + th.pad_xs, base, label ? label : "", true);
        }

        const char*   v  = value ? "ON" : "OFF";
        const int16_t w  = 6 * 3;
        const auto    tx = (int16_t)(rc.x + rc.w - w - th.pad_xs);
        {
            const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
            r.drawText(*th.font_default, tx, base, v, true);
        }
    }

    template <class R>
    // Legacy overload: prefer RowParams version.
    void toggle_row(R& r, const Rect& rc, const char* label, const bool value, const bool focused, std::uint32_t now_ms,
                    const bool frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);

        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);

        const char*   v  = value ? "ON" : "OFF";
        const int16_t w  = 6 * 3;
        const auto    tx = (int16_t)(rc.x + rc.w - w - th.pad_xs);
        {
            const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
            r.drawText(*th.font_default, tx, base, v, true);
        }
    }

    template <class R>
    inline void toggle_row(R& r, const RowParams& p, const bool value) noexcept
    {
        toggle_row(r, p.rc, p.label, value, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    // Legacy overload: prefer RowParams version.
    void checkbox_row(R&            r, const Rect& rc, const char* label, const bool value, const bool focused,
                      std::uint32_t now_ms,
                      const bool    frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);

        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);

        const std::int16_t box_y = (std::int16_t)(rc.y + (rc.h - 7) / 2);
        const Rect         box{(std::int16_t)(rc.x + rc.w - 11), box_y, 7, 7};
        r.drawRect(box, true);
        if (value) {
            
            const Rect fill{(std::int16_t)(box.x + 2), (std::int16_t)(box.y + 2),
                               (std::int16_t)(box.w - 4), (std::int16_t)(box.h - 4)};
            if (fill.w > 0 && fill.h > 0) {
                r.fillRect(fill, true);
            }

        }
    }

    template <class R>
    inline void checkbox_row(R& r, const RowParams& p, const bool value) noexcept
    {
        checkbox_row(r, p.rc, p.label, value, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    // Legacy overload: prefer RowParams version.
    void switch_row(R& r, const Rect& rc, const char* label, const bool value, const bool focused, std::uint32_t now_ms,
                    const bool frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);

        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);

        const std::int16_t sw_y = (std::int16_t)(rc.y + (rc.h - 6) / 2);
        const Rect         sw{(std::int16_t)(rc.x + rc.w - 16), sw_y, 12, 6};
        r.drawRect(sw, true);
        const int knob_x = value ? (sw.x + sw.w - 5) : (sw.x + 1);
        r.fillRect(Rect{(std::int16_t)knob_x, (std::int16_t)(sw.y + 1), 4, 4}, true);
    }

    template <class R>
    inline void switch_row(R& r, const RowParams& p, const bool value) noexcept
    {
        switch_row(r, p.rc, p.label, value, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    void progress_row(R& r, const Rect& rc, const char* label, const uint8_t percent, const bool focused) noexcept
    {
        const auto& th = gui::theme::current();
        Rect highlight{};
        const Rect* invert = nullptr;
        if (focused && label_bg_rect(rc, label, th.pad_xs, kNoOverrideY, highlight)) {
            fill_round_rect(r, highlight);
            invert = &highlight;
        }
        r.drawRect(rc, true);

        const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
        if (invert) {
            gui::draw_text_masked(r, *th.font_default, rc.x + th.pad_xs, base, label ? label : "", true, *invert);
        } else {
            r.drawText(*th.font_default, rc.x + th.pad_xs, base, label ? label : "", true);
        }

        // 进度条区域
        const int barX = rc.x + 54;
        const int barY = rc.y + (rc.h - 9) / 2;
        const int barW = rc.w - 58;
        const int barH = 9;

        // 边框与文本一致：使用 !focused，使反色/非反色都清晰
        r.drawRect(Rect{(int16_t)barX, (int16_t)barY, (int16_t)barW, (int16_t)barH}, true);


        const int p    = (percent > 100) ? 100 : (int)percent;
        const int fill = (barW * p) / 100;
        if (fill > 2) {
            r.fillRect(Rect{(int16_t)(barX + 1), (int16_t)(barY + 1), (int16_t)(fill - 2), (int16_t)(barH - 2)},
                       true);
        }
    }

    template <class R>
    // Legacy overload: prefer RowParams version.
    void progress_row(R&            r, const Rect& rc, const char* label, const uint8_t percent, const bool focused,
                      std::uint32_t now_ms,
                      const bool    frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);

        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);

        const int barX = rc.x + 54;
        const int barY = rc.y + (rc.h - 9) / 2;
        const int barW = rc.w - 58;
        const int barH = 9;
        r.drawRect(Rect{(int16_t)barX, (int16_t)barY, (int16_t)barW, (int16_t)barH}, true);

        const int p    = (percent > 100) ? 100 : (int)percent;
        const int fill = (barW * p) / 100;
        if (fill > 2) {
            r.fillRect(Rect{(int16_t)(barX + 1), (int16_t)(barY + 1), (int16_t)(fill - 2), (int16_t)(barH - 2)},
                       true);
        }
    }

    template <class R>
    inline void progress_row(R& r, const RowParams& p, const std::uint8_t percent) noexcept
    {
        progress_row(r, p.rc, p.label, percent, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    // Legacy overload: prefer RowParams version.
    void chart_row(R&            r, const Rect& rc, const char* label, const ChartView& chart, const bool focused,
                   std::uint32_t now_ms,
                   const bool    frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);

        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);

        const int chartX = rc.x + 54;
        const int chartY = rc.y + (rc.h - 9) / 2;
        const int chartW = rc.w - 58;
        const int chartH = 9;
        r.drawRect(Rect{(int16_t)chartX, (int16_t)chartY, (int16_t)chartW, (int16_t)chartH}, true);

        const int n = (chart.count > 0) ? chart.count : 0;
        if (n <= 0 || !chart.data) return;
        const int barW = (chartW - 2) / n;
        for (int i = 0; i < n; ++i) {
            const int v = (int)chart.data[i];
            const int h = (chartH - 2) * v / 100;
            const int x = chartX + 1 + i * barW;
            const int y = chartY + chartH - 1 - h;
            r.fillRect(Rect{(int16_t)x, (int16_t)y, (int16_t)barW, (int16_t)h}, true);
        }
    }

    template <class R>
    inline void chart_row(R& r, const RowParams& p, const ChartView& chart) noexcept
    {
        chart_row(r, p.rc, p.label, chart, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    // Legacy overload: prefer RowParams version.
    template <class R>
    inline void value_row(R& r, const Rect& rc, const char* label, std::uint16_t value, const bool focused,
                          std::uint32_t now_ms,
                          const bool frame, int highlight_y, const Rect* invert) noexcept
    {
        value_row(r, rc, label, value, nullptr, focused, now_ms, frame, highlight_y, invert);
    }

    // Legacy overload: prefer RowParams version.
    template <class R>
    void value_row(R& r, const Rect& rc, const char* label, std::uint16_t value, const char* value_label,
                   const bool focused, std::uint32_t now_ms,
                   const bool frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);

        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);

        char buf[12]{};
        std::string_view text;
        if (value_label && value_label[0]) {
            text = detail::format_to<"{} {}">(buf, sizeof(buf),
                                             static_cast<unsigned>(value), value_label);
        } else {
            text = detail::format_to<"{}ms">(buf, sizeof(buf),
                                            static_cast<unsigned>(value));
        }
        const int text_w = gui::layout::text_width(*th.font_default, text);
        int x = rc.x + rc.w - th.pad_xs - text_w;
        if (x < rc.x + th.pad_xs) x = rc.x + th.pad_xs;
        const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
        if (invert) {
            gui::draw_text_masked(r, *th.font_default, x, base, text, true, *invert);
        } else {
            r.drawText(*th.font_default, x, base, text, true);
        }
    }

    template <class R>
    inline void value_row(R& r, const RowParams& p, std::uint16_t value, const char* value_label = nullptr) noexcept
    {
        value_row(r, p.rc, p.label, value, value_label, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    void value_text_row(R& r, const Rect& rc, const char* label, const char* value_text, const bool focused,
                        std::uint32_t now_ms,
                        const bool frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);

        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);

        const char* text = value_text ? value_text : "";
        const int text_w = gui::layout::text_width(*th.font_default, text);
        int x = rc.x + rc.w - th.pad_xs - text_w;
        if (x < rc.x + th.pad_xs) x = rc.x + th.pad_xs;
        const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
        if (invert) {
            gui::draw_text_masked(r, *th.font_default, x, base, text, true, *invert);
        } else {
            r.drawText(*th.font_default, x, base, text, true);
        }
    }

    template <class R>
    inline void value_text_row(R& r, const RowParams& p, const char* value_text) noexcept
    {
        value_text_row(r, p.rc, p.label, value_text, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    void range_row(R& r, const Rect& rc, const char* label, const std::uint8_t percent, const bool focused,
                   std::uint32_t now_ms,
                   const bool frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);

        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);

        const int barX = rc.x + 54;
        const int barY = rc.y + (rc.h - 7) / 2;
        const int barW = rc.w - 58;
        const int barH = 7;
        r.drawRect(Rect{(int16_t)barX, (int16_t)barY, (int16_t)barW, (int16_t)barH}, true);

        const int p = (percent > 100) ? 100 : (int)percent;
        const int knob_x = barX + 1 + (barW - 2) * p / 100;
        const int knob_y = barY + 1;
        r.fillRect(Rect{(std::int16_t)knob_x, (std::int16_t)knob_y, 2, (std::int16_t)(barH - 2)}, true);
    }

    template <class R>
    inline void range_row(R& r, const RowParams& p, const std::uint8_t percent) noexcept
    {
        range_row(r, p.rc, p.label, percent, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    void stepper_row(R& r, const Rect& rc, const char* label, std::uint16_t value, const char* value_label,
                     const bool focused,
                     std::uint32_t now_ms,
                     const bool frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);

        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);

        char buf[16]{};
        std::string_view text;
        if (value_label && value_label[0]) {
            text = detail::format_to<"{} {}">(buf, sizeof(buf),
                                             static_cast<unsigned>(value), value_label);
        } else {
            text = detail::format_to<"{}">(buf, sizeof(buf),
                                          static_cast<unsigned>(value));
        }
        const int text_w = gui::layout::text_width(*th.font_default, text);
        const int btn_w = 9;
        const int btn_h = 9;
        const int right = rc.x + rc.w - th.pad_xs;
        const int by = rc.y + (rc.h - btn_h) / 2;
        const Rect plus{(std::int16_t)(right - btn_w), (std::int16_t)by, (std::int16_t)btn_w, (std::int16_t)btn_h};
        const Rect minus{(std::int16_t)(right - btn_w - btn_w - 2), (std::int16_t)by,
                         (std::int16_t)btn_w, (std::int16_t)btn_h};
        r.drawRect(minus, true);
        r.drawRect(plus, true);
        const int minus_base = gui::layout::baseline_from_top(*th.font_default, minus.y + 1);
        r.drawText(*th.font_default, (std::int16_t)(minus.x + 2), (std::int16_t)minus_base, "-", true);
        const int plus_base = gui::layout::baseline_from_top(*th.font_default, plus.y + 1);
        r.drawText(*th.font_default, (std::int16_t)(plus.x + 2), (std::int16_t)plus_base, "+", true);

        int x = minus.x - 6 - text_w;
        if (x < rc.x + th.pad_xs) x = rc.x + th.pad_xs;
        const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
        if (invert) {
            gui::draw_text_masked(r, *th.font_default, x, base, text, true, *invert);
        } else {
            r.drawText(*th.font_default, x, base, text, true);
        }
    }

    template <class R>
    inline void stepper_row(R& r, const RowParams& p, std::uint16_t value, const char* value_label = nullptr) noexcept
    {
        stepper_row(r, p.rc, p.label, value, value_label, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    void section_row(R& r, const Rect& rc, const char* label) noexcept
    {
        const auto& th = gui::theme::current();
        const char* text = label ? label : "";
        const int base = gui::layout::row_baseline_centered(*th.font_default, rc);
        r.drawText(*th.font_default, rc.x + th.pad_xs, base, text, true);
        const int y = rc.y + rc.h - 1;
        draw_line(r, rc.x + th.pad_xs, y, rc.x + rc.w - th.pad_xs, y, true);
    }

    template <class R>
    void segmented_control(R& r, const Rect& rc, const char* const* labels, std::uint8_t count,
                           std::uint8_t selected) noexcept
    {
        if (!labels || count == 0 || rc.w <= 0 || rc.h <= 0) return;
        const auto& th = gui::theme::current();
        const int seg_w = rc.w / count;
        for (std::uint8_t i = 0; i < count; ++i) {
            Rect seg{
                (std::int16_t)(rc.x + i * seg_w),
                rc.y,
                (std::int16_t)((i == count - 1) ? (rc.w - i * seg_w) : seg_w),
                rc.h
            };
            const bool on = (i == selected);
            if (on) {
                fill_round_rect(r, seg);
            } else {
                r.drawRect(seg, true);
            }
            const char* text = labels[i] ? labels[i] : "";
            const int text_w = gui::layout::text_width(*th.font_default, text);
            const int x = seg.x + (seg.w - text_w) / 2;
            const int base = gui::layout::row_baseline_centered(*th.font_default, seg);
            r.drawText(*th.font_default, (std::int16_t)x, (std::int16_t)base, text, !on);
        }
    }

    template <class R>
    void segmented_row(R& r, const Rect& rc, const char* label,
                       const char* const* labels, std::uint8_t count, std::uint8_t selected,
                       const bool focused,
                       std::uint32_t now_ms,
                       const bool frame, int highlight_y, const Rect* invert) noexcept
    {
        const auto& th = gui::theme::current();
        (void)focused;
        (void)highlight_y;
        if (frame) r.drawRect(rc, true);
        draw_label_scrolling_masked(r, rc, label, now_ms, th.pad_xs, th.pad_sm, invert);
        const int seg_x = rc.x + 54;
        const int seg_w = rc.w - 58;
        const Rect seg_rc{(std::int16_t)seg_x, (std::int16_t)(rc.y + 2),
                          (std::int16_t)seg_w, (std::int16_t)(rc.h - 4)};
        segmented_control(r, seg_rc, labels, count, selected);
    }

    template <class R>
    inline void segmented_row(R& r, const RowParams& p, const char* const* labels,
                              std::uint8_t count, std::uint8_t selected) noexcept
    {
        segmented_row(r, p.rc, p.label, labels, count, selected, p.focused, p.now_ms, p.frame, p.highlight_y, p.invert);
    }

    template <class R>
    void draw_toast(R& r, const Rect& screen, const char* text) noexcept
    {
        const auto& th = gui::theme::current();
        if (!text || !th.font_default) return;
        const int text_w = gui::layout::text_width(*th.font_default, text);
        const int pad = 4;
        const int w = text_w + pad * 2;
        const int h = th.font_default->line_height + 4;
        int x = screen.x + (screen.w - w) / 2;
        int y = screen.y + screen.h - h - 4;
        if (x < screen.x) x = screen.x;
        const Rect rc{(std::int16_t)x, (std::int16_t)y, (std::int16_t)w, (std::int16_t)h};
        fill_round_rect(r, rc);
        const int base = gui::layout::baseline_from_top(*th.font_default, rc.y + 2);
        r.drawText(*th.font_default, (std::int16_t)(rc.x + pad), (std::int16_t)base, text, false);
        r.drawRect(rc, true);
    }
} // namespace gui

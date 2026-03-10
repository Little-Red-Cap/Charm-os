module;
#include <cstddef>
#include <cstring>
#include <expected>
#include <string_view>
export module charm.widgets.progress_bar_round;

import charm.core.object;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.text_box;
import out.core;
import out.format;
import out.sink;
import alg_arc;

using namespace ui::render;

namespace {
    struct trunc_sink {
        char* buf{nullptr};
        std::size_t cap{0};
        std::size_t pos{0};

        out::result<std::size_t> write(out::bytes b) noexcept {
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

    inline void append_sv(trunc_sink& sink, std::string_view sv) noexcept {
        (void)out::write(sink, sv);
    }

    inline void append_int(trunc_sink& sink, int value) noexcept {
        (void)out::vprint<"{}">(sink, value);
    }

    inline void append_uint(trunc_sink& sink, unsigned value) noexcept {
        (void)out::vprint<"{}">(sink, value);
    }

    inline std::string_view format_value(char* buf, std::size_t size,
                                         const char* fmt, int value) noexcept {
        if (!buf || size == 0) return {};
        if (!fmt || !*fmt) fmt = "%d";
        trunc_sink sink{buf, size - 1u, 0u};
        bool used = false;
        for (const char* p = fmt; *p; ) {
            if (*p != '%') {
                append_sv(sink, std::string_view{p, 1});
                ++p;
                continue;
            }
            ++p;
            if (*p == '%') {
                append_sv(sink, std::string_view{"%", 1});
                ++p;
                continue;
            }
            while (*p == 'l') ++p;
            const char spec = *p ? *p++ : 0;
            if (used) {
                append_sv(sink, std::string_view{"?", 1});
                continue;
            }
            if (spec == 'd' || spec == 'i') {
                append_int(sink, value);
            } else if (spec == 'u') {
                append_uint(sink, static_cast<unsigned>(value));
            } else {
                append_sv(sink, std::string_view{"?", 1});
            }
            used = true;
        }
        buf[sink.pos] = '\0';
        return {buf, sink.pos};
    }
}

// Round progress bar (ARM-2D progress_bar_round inspired)
export
class ProgressBarRound : public WidgetBase<ProgressBarRound> {
public:
    ProgressBarRound() {
        set_size(120, 120);
    }

    void set_value(int v) noexcept {
        value_ = alg::arc::clamp_to_range(v, 0, 100);
    }

    int value() const noexcept { return value_; }

    void set_thickness(int t) noexcept { thickness_ = (t > 0) ? t : 1; }
    void set_start_angle(int deg) noexcept { start_deg_ = deg; }
    void set_show_track(bool on) noexcept { show_track_ = on; }
    void set_fill_color(const rgba& c) noexcept { fill_color_ = c; }
    void set_track_color(const rgba& c) noexcept { track_color_ = c; }
    void set_show_value(bool on) noexcept { show_value_ = on; }
    void set_value_format(const char* fmt) noexcept { value_format_ = (fmt && *fmt) ? fmt : "%d"; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<ProgressBarRound>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::ProgressBarRound, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{}, border{}, font{};

        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int cx = r.x + r.w / 2;
        const int cy = r.y + r.h / 2;
        int radius = (r.w < r.h ? r.w : r.h) / 2 - thickness_ - 2;
        if (radius < 1) radius = 1;

        const int start = start_deg_;
        const int end = start + 360;
        const rgba track = track_color_.a ? track_color_ : border;
        const rgba fill = fill_color_.a ? fill_color_ : font;
        if (show_track_) {
            draw_arc(cvs, cx, cy, radius, thickness_, start, end, track);
        }

        const float sweep = alg::arc::sweep_deg_from_value(static_cast<float>(start),
                                                           static_cast<float>(end),
                                                           alg::arc::ratio_from_range(value_, 0, 100));
        draw_arc(cvs, cx, cy, radius, thickness_, start, sweep, fill);

        const float rad_start = alg::arc::deg_to_rad(static_cast<float>(start));
        const float rad_end = alg::arc::deg_to_rad(sweep);
        const auto p0 = alg::arc::point_on_circle_rad(cx, cy, radius, rad_start);
        const auto p1 = alg::arc::point_on_circle_rad(cx, cy, radius, rad_end);
        const int cap_r = (thickness_ > 2) ? (thickness_ / 2) : 1;
        draw_circle(cvs, p0.x, p0.y, cap_r, fill, true);
        draw_circle(cvs, p1.x, p1.y, cap_r, fill, true);

        if (show_value_) {
            char buf[16]{};
            (void)format_value(buf, sizeof(buf), value_format_, value_);
            draw_text_box(cvs, r, buf, font, resolve_font(st),
                          TextAlignH::Center, TextAlignV::Center,
                          TextWrap::None, TextEllipsis::None);
        }
    }

private:
    int value_{0};
    int thickness_{6};
    int start_deg_{-90};
    bool show_track_{true};
    bool show_value_{false};
    const char* value_format_{"%d"};
    rgba fill_color_{0, 0, 0, 0};
    rgba track_color_{0, 0, 0, 0};
};





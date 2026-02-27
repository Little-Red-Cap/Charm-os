module;
#include <algorithm>
export module charm.widgets.scrollbar;

import charm.core.object;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;
import charm.core.style_sheet;

using namespace ui::render;

export
class ScrollBar : public WidgetBase<ScrollBar> {
public:
    enum class Orientation {
        Horizontal,
        Vertical
    };

    ScrollBar() {
        set_focusable(true);
        set_size(160, 12);
    }

    void set_orientation(Orientation o) noexcept { orient_ = o; }
    Orientation orientation() const noexcept { return orient_; }

    void set_range(int min_v, int max_v) noexcept {
        if (min_v > max_v) std::swap(min_v, max_v);
        min_ = min_v;
        max_ = max_v;
        value_ = clamp(value_);
    }

    void set_page_size(int v) noexcept {
        page_size_ = (v > 0) ? v : 1;
    }

    void set_value(int v) noexcept {
        const int clamped = clamp(v);
        if (value_ == clamped) return;
        value_ = clamped;
        if (on_change_) on_change_();
    }

    int value() const noexcept { return value_; }

    void set_on_change(Callback cb) noexcept { on_change_ = cb; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<ScrollBar>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::ScrollBar, state, base, st_scratch);
        const auto r = get_rect();

        rgba track{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, track, border, font);
        const rgba thumb = resolve_accent(st, state);

        const Rect track_rect = calc_track_rect(st);
        draw_rect(cvs, track_rect.x, track_rect.y, track_rect.w, track_rect.h, track, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const Rect thumb_rect = calc_thumb_rect(st);
        draw_rect(cvs, thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h, thumb, true);

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        const auto r = get_rect();
        if (e.type == Event::Type::MouseDown) {
            if (!r.contains(e.x, e.y)) return false;
            const Style& st = Theme::instance().get<ScrollBar>();
            drag_start_pos_ = (orient_ == Orientation::Horizontal) ? e.x : e.y;
            drag_start_value_ = value_;
            const Rect thumb = calc_thumb_rect(st);
            dragging_ = thumb.contains(e.x, e.y);
            if (!dragging_) {
                const int step = page_size_ > 0 ? page_size_ : 1;
                if ((orient_ == Orientation::Horizontal && e.x < thumb.x) ||
                    (orient_ == Orientation::Vertical && e.y < thumb.y)) {
                    set_value(value_ - step);
                } else {
                    set_value(value_ + step);
                }
            }
            return true;
        } else if (e.type == Event::Type::DragStart || e.type == Event::Type::DragMove) {
            if (!dragging_) return false;
            const int cur = (orient_ == Orientation::Horizontal) ? e.x : e.y;
            const int delta = cur - drag_start_pos_;
            const Style& st = Theme::instance().get<ScrollBar>();
            const int span = track_span(st);
            if (span > 0) {
                const int range = max_ - min_;
                const int dv = (range > 0) ? (delta * range) / span : 0;
                set_value(drag_start_value_ + dv);
            }
            return true;
        } else if (e.type == Event::Type::DragEnd || e.type == Event::Type::MouseUp) {
            dragging_ = false;
            return true;
        }
        return false;
    }

private:
    int clamp(int v) const noexcept {
        if (v < min_) return min_;
        if (v > max_) return max_;
        return v;
    }

    Rect calc_track_rect(const Style& st) const noexcept {
        const auto r = get_rect();
        int margin = st.scrollbar_margin;
        if (margin < 0) margin = 0;
        Rect out{r.x + margin, r.y + margin, r.w - margin * 2, r.h - margin * 2};
        if (out.w < 1) out.w = 1;
        if (out.h < 1) out.h = 1;
        return out;
    }

    int track_span(const Style& st) const noexcept {
        const auto track = calc_track_rect(st);
        const int track_len = (orient_ == Orientation::Horizontal) ? track.w : track.h;
        const int thumb_len = thumb_length(track_len, st);
        return track_len - thumb_len;
    }

    int thumb_length(int track_len, const Style& st) const noexcept {
        const int range = max_ - min_;
        if (range <= 0) return track_len;
        const int total = range + page_size_;
        int len = (track_len * page_size_) / total;
        int min_len = st.scrollbar_thumb_min;
        if (min_len < 2) min_len = 2;
        if (len < min_len) len = min_len;
        if (len > track_len) len = track_len;
        return len;
    }

    Rect calc_thumb_rect(const Style& st) const noexcept {
        const auto track = calc_track_rect(st);
        const int track_len = (orient_ == Orientation::Horizontal) ? track.w : track.h;
        const int thumb_len = thumb_length(track_len, st);
        const int span = track_len - thumb_len;
        const int range = max_ - min_;
        const int offset = (range > 0 && span > 0) ? ((value_ - min_) * span) / range : 0;
        if (orient_ == Orientation::Horizontal) {
            return Rect{track.x + offset, track.y, thumb_len, track.h};
        }
        return Rect{track.x, track.y + offset, track.w, thumb_len};
    }

    Orientation orient_{Orientation::Horizontal};
    int min_{0};
    int max_{100};
    int value_{0};
    int page_size_{10};
    bool dragging_{false};
    int drag_start_pos_{0};
    int drag_start_value_{0};
    Callback on_change_{};
};





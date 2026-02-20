module;
#include <algorithm>
export module charm.widgets.scrollbar;

import charm.core.object;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;

using namespace ui::render;

export
class ScrollBar : public ObjectBase {
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

    void draw(CanvasBase& cvs) override {
        const Style& st = Theme::instance().get<ScrollBar>();
        const auto r = get_rect();

        rgba track{};
        rgba border{};
        rgba thumb{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       track, border, thumb);

        draw_rect(cvs, r.x, r.y, r.w, r.h, track, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const Rect thumb_rect = calc_thumb_rect();
        draw_rect(cvs, thumb_rect.x, thumb_rect.y, thumb_rect.w, thumb_rect.h, thumb, true);

        if (has_state(State::Focused)) {
            draw_rect(cvs, r.x, r.y, r.w, r.h, st.border_focus, false);
        }
    }

    bool on_event(const Event& e) override {
        if (!is_enabled()) return false;
        const auto r = get_rect();
        if (e.type == Event::Type::MouseDown) {
            if (!r.contains(e.x, e.y)) return false;
            drag_start_pos_ = (orient_ == Orientation::Horizontal) ? e.x : e.y;
            drag_start_value_ = value_;
            const Rect thumb = calc_thumb_rect();
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
            const int span = track_span();
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

    int track_span() const noexcept {
        const auto r = get_rect();
        const int track_len = (orient_ == Orientation::Horizontal) ? r.w : r.h;
        const int thumb_len = thumb_length(track_len);
        return track_len - thumb_len;
    }

    int thumb_length(int track_len) const noexcept {
        const int range = max_ - min_;
        if (range <= 0) return track_len;
        const int total = range + page_size_;
        int len = (track_len * page_size_) / total;
        if (len < 8) len = 8;
        if (len > track_len) len = track_len;
        return len;
    }

    Rect calc_thumb_rect() const noexcept {
        const auto r = get_rect();
        const int track_len = (orient_ == Orientation::Horizontal) ? r.w : r.h;
        const int thumb_len = thumb_length(track_len);
        const int span = track_len - thumb_len;
        const int range = max_ - min_;
        const int offset = (range > 0 && span > 0) ? ((value_ - min_) * span) / range : 0;
        if (orient_ == Orientation::Horizontal) {
            return Rect{r.x + offset, r.y, thumb_len, r.h};
        }
        return Rect{r.x, r.y + offset, r.w, thumb_len};
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

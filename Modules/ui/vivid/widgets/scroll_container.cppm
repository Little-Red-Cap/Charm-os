module;
#include <cstddef>
#include <algorithm>
#include <cstdlib>
export module charm.widgets.scroll_container;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.gfx.color;
import charm.gfx.render;
import charm.gfx.image;
import charm.widgets.text;
import charm.font.typography;
import charm.core.style;

using namespace ui::render;

export
class ScrollContainer : public ObjectBase {
public:
    ScrollContainer() {
        set_focusable(true);
    }
    void set_scroll_y(int y) noexcept {
        scroll_y_ = clamp_scroll(y);
        velocity_ = 0;
    }

    void add_scroll_y(int dy) noexcept {
        scroll_y_ = clamp_scroll(scroll_y_ + dy);
    }

    int scroll_y() const noexcept { return scroll_y_; }

    void set_wheel_step(int step) noexcept { wheel_step_ = step; }
    void set_deceleration(float d) noexcept { decel_ = d; }
    void set_drag_threshold(int px) noexcept { drag_threshold_sq_ = px * px; }

    template<typename Resolver>
    void sync_child_bases(Resolver&& resolve) {
        content_height_ = 0;
        const std::size_t total = child_count();
        overflow_ = total > kMax;
        for (std::size_t i = 0; i < total; ++i) {
            auto h = child_at(i);
            auto* ch = resolve(h);
            if (!ch) continue;
            const auto r = ch->get_rect();
            if (i < kMax) {
                base_x_[i] = r.x - get_rect().x;
                base_y_[i] = r.y - get_rect().y;
            }
            const int base_y = (i < kMax) ? base_y_[i] : (r.y - get_rect().y + scroll_y_);
            const int bottom = base_y + r.h;
            if (bottom > content_height_) content_height_ = bottom;
        }
        update_scroll_bounds();
        apply_scroll(resolve, false);
    }

    void draw(DefaultCanvas& cvs) override {
        const auto r = get_rect();
        const Style& st = has_local_style_ ? style_ : Theme::instance().get<ScrollContainer>();
        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), dragging_, has_state(State::Focused)},
                       bg, border, font);

        if (has_skin_) {
            draw_image_nine_slice(cvs, r.x, r.y, r.w, r.h, skin_,
                                  slice_left_, slice_top_, slice_right_, slice_bottom_);
            draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        } else {
            draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
            draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        }

        if (has_state(State::Focused)) {
            draw_rect(cvs, r.x, r.y, r.w, r.h, st.border_focus, false);
        }

        if (content_height_ > r.h) {
            const int track_w = 6;
            const int track_x = r.x + r.w - track_w - 2;
            const int track_h = r.h - 4;
            const float ratio = static_cast<float>(r.h) / static_cast<float>(content_height_);
            int thumb_h = static_cast<int>(track_h * ratio);
            if (thumb_h < 12) thumb_h = 12;
            const float tpos = (max_scroll_ > 0) ? (static_cast<float>(scroll_y_) / static_cast<float>(max_scroll_)) : 0.0f;
            const int thumb_y = r.y + 2 + static_cast<int>((track_h - thumb_h) * tpos);
            rgba thumb = st.border_focus;
            thumb.a = 180;
            draw_rect(cvs, track_x, r.y + 2, track_w, track_h, rgba{0,0,0,0}, false);
            draw_rect(cvs, track_x, thumb_y, track_w, thumb_h, thumb, true);
        }

        if (dragging_) {
            Rect hint{r.x + 8, r.y + 4, r.w - 16, 18};
            draw_text_box(cvs, hint, "Dragging", {60, 60, 70, 255},
                          resolve_font(st),
                          TextAlignH::Right, TextAlignV::Top,
                          TextWrap::None, TextEllipsis::None);
        }
    }

    bool on_event(const Event& e) override {
        const auto r = get_rect();
        if (e.type == Event::Type::MouseDown) {
            if (!r.contains(e.x, e.y)) return false;
            dragging_ = true;
            last_y_ = e.y;
            velocity_ = 0;
            return true;
        } else if (e.type == Event::Type::DragStart) {
            if (!r.contains(e.x, e.y)) return false;
            dragging_ = true;
            last_y_ = e.y;
            velocity_ = 0;
            return true;
        } else if (e.type == Event::Type::DragMove) {
            if (dragging_) {
                const int dy = (e.dy != 0) ? e.dy : (e.y - last_y_);
                last_y_ = e.y;
                set_scroll_y(scroll_y_ - dy);
                velocity_ = -dy; // simple velocity estimate per tick
                return true;
            }
        } else if (e.type == Event::Type::MouseMove) {
            if (dragging_) {
                const int dy = e.y - last_y_;
                last_y_ = e.y;
                set_scroll_y(scroll_y_ - dy);
                velocity_ = -dy;
                return true;
            }
        } else if (e.type == Event::Type::MouseUp) {
            if (!dragging_) return false;
            dragging_ = false;
            if (velocity_ != 0 && (velocity_ * velocity_) < drag_threshold_sq_) velocity_ = 0;
            return true;
        } else if (e.type == Event::Type::DragEnd) {
            if (!dragging_) return false;
            dragging_ = false;
            if (velocity_ != 0 && (velocity_ * velocity_) < drag_threshold_sq_) velocity_ = 0;
            return true;
        } else if (e.type == Event::Type::MouseWheel) {
            if (!r.contains(e.x, e.y)) return false;
            const int target_step = e.wheel_y * wheel_step_;
            add_scroll_y(-target_step);
            velocity_ = -target_step;
            return true;
        }
        return false;
    }

    template<typename Resolver>
    void apply_scroll(Resolver&& resolve, bool advance = true) {
        const auto r = get_rect();
        const std::size_t total = child_count();
        overflow_ = total > kMax;
        for (std::size_t i = 0; i < total; ++i) {
            auto h = child_at(i);
            auto* ch = resolve(h);
            if (!ch) continue;
            const int base_x = (i < kMax) ? base_x_[i] : (ch->get_rect().x - r.x);
            const int base_y = (i < kMax) ? base_y_[i] : (ch->get_rect().y - r.y + scroll_y_);
            ch->set_pos(r.x + base_x, r.y + base_y - scroll_y_);
        }
        if (advance && !dragging_ && velocity_ != 0) {
            add_scroll_y(velocity_);
            velocity_ = static_cast<int>(static_cast<float>(velocity_) * decel_);
            if (std::abs(velocity_) < 1) velocity_ = 0;
        }
    }

    void set_style(rgba bg, rgba border) noexcept {
        style_.bg_color = bg;
        style_.border_color = border;
        has_local_style_ = true;
    }

    void set_skin(const ImageView& img, int left, int top, int right, int bottom) noexcept {
        skin_ = img;
        slice_left_ = left;
        slice_top_ = top;
        slice_right_ = right;
        slice_bottom_ = bottom;
        has_skin_ = true;
    }

    bool should_draw_child(const ObjectBase& ch) const noexcept override {
        const auto r = get_rect();
        const auto c = ch.get_rect();
        return !(c.x + c.w <= r.x || c.x >= r.x + r.w ||
                 c.y + c.h <= r.y || c.y >= r.y + r.h);
    }

private:
    int clamp_scroll(int y) const noexcept {
        if (y < 0) return 0;
        if (y > max_scroll_) return max_scroll_;
        return y;
    }

    void update_scroll_bounds() {
        const auto r = get_rect();
        const int max = content_height_ - r.h;
        max_scroll_ = (max > 0) ? max : 0;
    }

    static constexpr std::size_t kMax = 64;
    int base_x_[kMax]{};
    int base_y_[kMax]{};
    int content_height_{0};

    int scroll_y_{0};
    int max_scroll_{0};
    int wheel_step_{24};
    float decel_{0.85f};
    int drag_threshold_sq_{9};
    bool dragging_{false};
    int last_y_{0};
    int velocity_{0};

    Style style_{};
    bool has_local_style_{false};
    ImageView skin_{};
    bool has_skin_{false};
    int slice_left_{0};
    int slice_top_{0};
    int slice_right_{0};
    int slice_bottom_{0};
    bool overflow_{false};
};

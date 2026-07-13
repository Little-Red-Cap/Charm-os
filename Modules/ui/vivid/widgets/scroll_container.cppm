module;
#include <cstddef>
#include <cstdlib>
export module charm.widgets.scroll_container;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.core.input_interaction;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.gfx.image;
import charm.gfx.text_box;
import charm.font.typography;
import charm.core.style;
import charm.core.style_sheet;
import alg_scroll_bounds;
import alg_scroll_thumb;

using namespace ui::render;

export
class ScrollContainer : public WidgetBase<ScrollContainer, 64> {
public:
    ScrollContainer() {
        set_focusable(true);
        pinch_strategy_.set_callbacks(PinchScrollStrategy::begin_delegate::bind<&ScrollContainer::on_pinch_begin>(*this),
                                      PinchScrollStrategy::update_delegate::bind<&ScrollContainer::on_pinch_update>(*this),
                                      PinchScrollStrategy::end_delegate::bind<&ScrollContainer::on_pinch_end>(*this));
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
    void set_pinch_enabled(bool on) noexcept {
        pinch_strategy_.set_enabled(on);
    }
    void set_scroll_hint_enabled(bool on) noexcept { show_scroll_hint_ = on; }

    template<typename Resolver>
    void sync_child_bases(Resolver&& resolve) {
        const auto container = get_rect();
        content_height_ = 0;
        const std::size_t total = child_count();
        base_count_ = (total < kMax) ? total : kMax;
        for (std::size_t i = 0; i < total; ++i) {
            auto h = child_at(i);
            auto* ch = resolve(h);
            if (!ch) continue;
            const auto child_rect = ch->get_rect();
            int base_x = child_rect.x - container.x;
            int base_y = child_rect.y - container.y + scroll_y_;
            if (i < base_count_) {
                base_x_[i] = base_x;
                base_y_[i] = base_y;
                base_x = base_x_[i];
                base_y = base_y_[i];
            }
            const int bottom = base_y + child_rect.h;
            if (bottom > content_height_) content_height_ = bottom;
        }
        update_scroll_bounds();
        apply_scroll(resolve, false);
    }

    void draw(CanvasBase& cvs) {
        const auto r = get_rect();
        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = has_local_style_ ? style_ : Theme::instance().get<ScrollContainer>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::ScrollContainer, state, base, st_scratch);
        resolve_colors(st, state, bg, border, font);

        if (has_skin_) {
            draw_image_nine_slice(cvs, r.x, r.y, r.w, r.h, skin_,
                                  slice_left_, slice_top_, slice_right_, slice_bottom_);
            draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        } else {
            draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
            draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);
        }

        draw_focus_ring(cvs, r, st, has_state(State::Focused));

        if (max_scroll_ > 0) {
            const int margin = (st.metrics.scrollbar_margin >= 0) ? st.metrics.scrollbar_margin : 0;
            const int track_w = 6;
            const int track_x = r.x + r.w - track_w - margin;
            const int track_y = r.y + margin;
            const int track_h = r.h - margin * 2;
            const auto thumb = alg::scroll_thumb::vertical_from_maxscroll(
                track_x, track_y, track_w, track_h, r.h, max_scroll_, scroll_y_, st.metrics.scrollbar_thumb_min);
            if (thumb.visible && thumb.thumb_h > 0) {
                rgba thumb_col = st.colors.border_focus;
                thumb_col.a = 180;
                draw_rect(cvs, track_x, track_y, track_w, track_h, rgba{0,0,0,0}, false);
                draw_rect(cvs, thumb.thumb_x, thumb.thumb_y, thumb.thumb_w, thumb.thumb_h, thumb_col, true);
            }
        }

        if (show_scroll_hint_ && dragging_) {
            Rect hint{r.x + 8, r.y + 4, r.w - 16, 18};
            draw_text_box(cvs, hint, "Dragging", {60, 60, 70, 255},
                          resolve_font(st),
                          TextAlignH::Right, TextAlignV::Top,
                          TextWrap::None, TextEllipsis::None);
        }
    }

    bool on_event(const Event& e) {
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
        } else if (e.type == Event::Type::GestureSwipe) {
            if (!r.contains(e.x, e.y)) return false;
            if (e.gesture_phase == Event::GesturePhase::Begin) {
                swipe_active_ = true;
                velocity_ = 0;
            } else if (e.gesture_phase == Event::GesturePhase::Update) {
                add_scroll_y(-e.dy);
                velocity_ = -e.dy;
            } else if (e.gesture_phase == Event::GesturePhase::End) {
                swipe_active_ = false;
            }
            return true;
        } else if (e.type == Event::Type::GesturePinch) {
            if (!r.contains(e.x, e.y)) return false;
            return pinch_strategy_.on_event(e);
        }
        return false;
    }

    template<typename Resolver>
    void apply_scroll(Resolver&& resolve, bool advance = true) {
        const auto r = get_rect();
        const std::size_t total = child_count();
        for (std::size_t i = 0; i < total; ++i) {
            auto h = child_at(i);
            auto* ch = resolve(h);
            if (!ch) continue;
            int base_x = ch->get_rect().x - r.x;
            int base_y = ch->get_rect().y - r.y + scroll_y_;
            if (i < base_count_) {
                base_x = base_x_[i];
                base_y = base_y_[i];
            }
            ch->set_pos(r.x + base_x, r.y + base_y - scroll_y_);
        }
        if (advance && !dragging_ && velocity_ != 0) {
            const int next = clamp_scroll(scroll_y_ + velocity_);
            scroll_y_ = next;
            velocity_ = static_cast<int>(static_cast<float>(velocity_) * decel_);
            if (std::abs(velocity_) < 1) velocity_ = 0;
        }
    }

    void set_style(rgba bg, rgba border) noexcept {
        style_.colors.bg_color = bg;
        style_.colors.border_color = border;
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

private:
    void on_pinch_begin() noexcept {
        pinch_active_ = true;
        velocity_ = 0;
    }

    void on_pinch_update(int dy) noexcept {
        add_scroll_y(-dy);
        velocity_ = -dy;
    }

    void on_pinch_end() noexcept {
        pinch_active_ = false;
    }

    int clamp_scroll(int y) const noexcept {
        return alg::scroll_bounds::clamp(y, max_scroll_);
    }

    void update_scroll_bounds() {
        const auto r = get_rect();
        max_scroll_ = alg::scroll_bounds::compute_max(content_height_, r.h);
    }

    static constexpr std::size_t kMax = 64;
    int base_x_[kMax]{};
    int base_y_[kMax]{};
    std::size_t base_count_{0};
    int content_height_{0};

    int scroll_y_{0};
    int max_scroll_{0};
    int wheel_step_{24};
    float decel_{0.85f};
    int drag_threshold_sq_{9};
    bool dragging_{false};
    bool swipe_active_{false};
    bool pinch_active_{false};
    PinchScrollStrategy pinch_strategy_{};
    bool show_scroll_hint_{true};
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
};





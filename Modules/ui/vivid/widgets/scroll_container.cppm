module;
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <span>
export module charm.widgets.scroll_container;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.core.input_interaction;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.style;
import charm.core.style_sheet;
import alg_scroll_bounds;
import alg_scroll_thumb;

using namespace ui::render;

export
class ScrollContainer : public WidgetBase<ScrollContainer, std::dynamic_extent> {
public:
    ScrollContainer() {
        set_focusable(true);
        pinch_strategy_.set_callbacks(PinchScrollStrategy::begin_delegate::bind<&ScrollContainer::on_pinch_begin>(*this),
                                      PinchScrollStrategy::update_delegate::bind<&ScrollContainer::on_pinch_update>(*this),
                                      PinchScrollStrategy::end_delegate::bind<&ScrollContainer::on_pinch_end>(*this));
    }

    ScrollContainer(const ScrollContainer&) = delete;
    ScrollContainer& operator=(const ScrollContainer&) = delete;
    ScrollContainer(ScrollContainer&&) = delete;
    ScrollContainer& operator=(ScrollContainer&&) = delete;

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

    // Captures the post-layout origin. Scrolling then becomes one common
    // translation instead of a resident base position for every child.
    template<typename Resolver>
    bool sync_child_bases(Resolver&& resolve) {
        const auto container = get_rect();
        const std::size_t total = child_count();
        int next_content_height = 0;
        for (std::size_t i = 0; i < total; ++i) {
            auto h = child_at(i);
            auto* ch = resolve(h);
            if (!ch) return false;
            const auto child_rect = ch->get_rect();
            const int base_y = child_rect.y - container.y + scroll_y_;
            const int bottom = base_y + child_rect.h;
            if (bottom > next_content_height) next_content_height = bottom;
        }

        content_height_ = next_content_height;
        layout_origin_x_ = container.x;
        layout_origin_y_ = container.y;
        applied_scroll_y_ = scroll_y_;
        layout_valid_ = true;
        update_scroll_bounds();
        return true;
    }

    void draw(CanvasBase& cvs) {
        const auto r = get_rect();
        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<ScrollContainer>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::ScrollContainer, state, base, st_scratch);
        resolve_colors(st, state, bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

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
    bool apply_scroll(Resolver&& resolve, bool advance = true) {
        if (!layout_valid_) return false;

        const auto r = get_rect();
        const std::size_t total = child_count();

        // Resolve the entire set first so a missing child cannot leave a
        // partially translated tree.
        for (std::size_t i = 0; i < total; ++i) {
            if (!resolve(child_at(i))) return false;
        }

        const int dx = r.x - layout_origin_x_;
        const int dy = r.y - layout_origin_y_ - (scroll_y_ - applied_scroll_y_);
        if (dx != 0 || dy != 0) {
            for (std::size_t i = 0; i < total; ++i) {
                auto* child = resolve(child_at(i));
                assert(child && "ScrollContainer resolver changed during apply_scroll");
                if (!child) return false;
                const auto child_rect = child->get_rect();
                child->set_pos(child_rect.x + dx, child_rect.y + dy);
            }
        }

        layout_origin_x_ = r.x;
        layout_origin_y_ = r.y;
        applied_scroll_y_ = scroll_y_;

        if (advance && !dragging_ && velocity_ != 0) {
            const int next = clamp_scroll(scroll_y_ + velocity_);
            scroll_y_ = next;
            velocity_ = static_cast<int>(static_cast<float>(velocity_) * decel_);
            if (std::abs(velocity_) < 1) velocity_ = 0;
        }
        return true;
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

    int content_height_{0};
    int layout_origin_x_{0};
    int layout_origin_y_{0};
    int applied_scroll_y_{0};
    bool layout_valid_{false};

    int scroll_y_{0};
    int max_scroll_{0};
    int wheel_step_{24};
    float decel_{0.85f};
    int drag_threshold_sq_{9};
    bool dragging_{false};
    bool swipe_active_{false};
    bool pinch_active_{false};
    PinchScrollStrategy pinch_strategy_{};
    int last_y_{0};
    int velocity_{0};
};





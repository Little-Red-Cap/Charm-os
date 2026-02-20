module;
#include <cstddef>
#include <algorithm>
#include <cstdlib>
export module charm.widgets.scroll_container;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.core.input_interaction;
import charm.gfx.color;
import charm.gfx.render;
import charm.gfx.image;
import charm.widgets.text;
import charm.font.typography;
import charm.core.style;
import alg_scroll;

using namespace ui::render;

export
class ScrollContainer : public ObjectBase {
public:
    static constexpr int kLayoutId = 1;

    ScrollContainer() {
        set_focusable(true);
        set_clip_policy(ClipPolicy::Custom);
        set_custom_layout(kLayoutId);
        pinch_strategy_.set_callbacks(&ScrollContainer::on_pinch_begin,
                                      &ScrollContainer::on_pinch_update,
                                      &ScrollContainer::on_pinch_end,
                                      this);
        enable_interaction(&pinch_strategy_, InteractionList<>::mask(Event::Type::GesturePinch));
    }
    void set_scroll_y(int y) noexcept {
        const int old = scroll_y_;
        scroll_y_ = clamp_scroll(y);
        velocity_ = 0;
        mark_scroll_dirty(old, scroll_y_);
    }

    void add_scroll_y(int dy) noexcept {
        const int old = scroll_y_;
        scroll_y_ = clamp_scroll(scroll_y_ + dy);
        mark_scroll_dirty(old, scroll_y_);
    }

    int scroll_y() const noexcept { return scroll_y_; }

    void set_wheel_step(int step) noexcept { wheel_step_ = step; }
    void set_deceleration(float d) noexcept { decel_ = d; }
    void set_drag_threshold(int px) noexcept { drag_threshold_sq_ = px * px; }
    void set_inertia_fast_ratio(float v) noexcept { inertia_fast_ratio_ = clamp_ratio(v); }
    void set_inertia_medium_ratio(float v) noexcept { inertia_medium_ratio_ = clamp_ratio(v); }
    void set_inertia_extra_ratio(float v) noexcept { inertia_extra_ratio_ = clamp_ratio(v); }
    void set_pinch_enabled(bool on) noexcept {
        pinch_strategy_.set_enabled(on);
        if (on) {
            enable_interaction(&pinch_strategy_, InteractionList<>::mask(Event::Type::GesturePinch));
        } else {
            disable_interaction(&pinch_strategy_);
        }
    }
    void set_scroll_hint_enabled(bool on) noexcept { show_scroll_hint_ = on; }

    template<typename Resolver>
    void sync_child_bases(Resolver&& resolve) {
        const auto container = get_rect();
        content_height_ = 0;
        const std::size_t total = child_count();
        base_count_ = (total < kMax) ? total : kMax;
        for (std::size_t i = 0; i < base_count_; ++i) {
            auto h = child_at(i);
            auto* ch = resolve(h);
            if (!ch) continue;
            const auto child_rect = ch->get_rect();
            base_x_[i] = child_rect.x - container.x;
            base_y_[i] = child_rect.y - container.y;
            const int bottom = base_y_[i] + child_rect.h;
            if (bottom > content_height_) content_height_ = bottom;
        }
        update_scroll_bounds();
        apply_scroll(resolve);
    }

    void draw(CanvasBase& cvs) override {
        const auto r = get_rect();
        const Style& st = has_local_style_ ? style_ : Theme::instance().get<ScrollContainer>();
        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), dragging_, has_state(State::Focused)},
                       bg, border, font);

        flush_scroll_dirty();

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

        if (show_scroll_hint_ && dragging_) {
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
            dragging_ = false;
            if (velocity_ != 0 && (velocity_ * velocity_) < drag_threshold_sq_) velocity_ = 0;
            return true;
        } else if (e.type == Event::Type::DragEnd) {
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
            return dispatch_interactions(e);
        }
        return false;
    }

    template<typename Resolver>
    void apply_scroll(Resolver&& resolve) {
        const auto r = get_rect();
        for (std::size_t i = 0; i < base_count_; ++i) {
            auto h = child_at(i);
            auto* ch = resolve(h);
            if (!ch) continue;
            ch->set_pos(r.x + base_x_[i], r.y + base_y_[i] - scroll_y_);
        }
        if (!dragging_ && velocity_ != 0) {
            const int old = scroll_y_;
            const int next = clamp_scroll(scroll_y_ + velocity_);
            if (next != old) {
                mark_scroll_dirty_inertia(old, next, (velocity_ < 0) ? -velocity_ : velocity_);
                scroll_y_ = next;
            }
            velocity_ = static_cast<int>(static_cast<float>(velocity_) * decel_);
            if (std::abs(velocity_) < 1) velocity_ = 0;
        }
    }

    void set_style(rgba bg, rgba border) noexcept {
        style_.bg_color = bg;
        style_.border_color = border;
        has_local_style_ = true;
        if (has_skin_) {
            update_clip_insets_for_skin();
        }
    }

    void set_skin(const ImageView& img, int left, int top, int right, int bottom) noexcept {
        skin_ = img;
        slice_left_ = left;
        slice_top_ = top;
        slice_right_ = right;
        slice_bottom_ = bottom;
        has_skin_ = true;
        update_clip_insets_for_skin();
    }

    bool should_draw_child(const ObjectBase& ch) const noexcept override {
        const auto r = children_clip_rect();
        const auto c = ch.get_rect();
        return !(c.x + c.w <= r.x || c.x >= r.x + r.w ||
                 c.y + c.h <= r.y || c.y >= r.y + r.h);
    }

    Rect children_clip_rect() const noexcept override {
        const auto r = get_rect();
        int left = clip_inset_left_;
        int top = clip_inset_top_;
        int right = clip_inset_right_;
        int bottom = clip_inset_bottom_;
        if (has_skin_) {
            if (slice_left_ > left) left = slice_left_;
            if (slice_top_ > top) top = slice_top_;
            if (slice_right_ > right) right = slice_right_;
            if (slice_bottom_ > bottom) bottom = slice_bottom_;
        }
        Rect inner{r.x + left, r.y + top, r.w - left - right, r.h - top - bottom};
        if (inner.w < 0) inner.w = 0;
        if (inner.h < 0) inner.h = 0;
        return inner;
    }

    void set_clip_insets(int left, int top, int right, int bottom) noexcept {
        clip_inset_left_ = (left >= 0) ? left : 0;
        clip_inset_top_ = (top >= 0) ? top : 0;
        clip_inset_right_ = (right >= 0) ? right : 0;
        clip_inset_bottom_ = (bottom >= 0) ? bottom : 0;
    }

private:
    static alg::scroll::Rect to_alg_rect(const Rect& r) noexcept {
        return alg::scroll::Rect{r.x, r.y, r.w, r.h};
    }

    static Rect from_alg_rect(const alg::scroll::Rect& r) noexcept {
        return Rect{r.x, r.y, r.w, r.h};
    }

    void accumulate_scroll_dirty(const Rect& r) noexcept {
        if (r.w <= 0 || r.h <= 0) return;
        if (!scroll_dirty_valid_) {
            scroll_dirty_accum_ = r;
            scroll_dirty_valid_ = true;
            return;
        }
        const int left = (r.x < scroll_dirty_accum_.x) ? r.x : scroll_dirty_accum_.x;
        const int top = (r.y < scroll_dirty_accum_.y) ? r.y : scroll_dirty_accum_.y;
        const int right = ((r.x + r.w) > (scroll_dirty_accum_.x + scroll_dirty_accum_.w))
            ? (r.x + r.w)
            : (scroll_dirty_accum_.x + scroll_dirty_accum_.w);
        const int bottom = ((r.y + r.h) > (scroll_dirty_accum_.y + scroll_dirty_accum_.h))
            ? (r.y + r.h)
            : (scroll_dirty_accum_.y + scroll_dirty_accum_.h);
        scroll_dirty_accum_.x = left;
        scroll_dirty_accum_.y = top;
        scroll_dirty_accum_.w = right - left;
        scroll_dirty_accum_.h = bottom - top;
    }

    void flush_scroll_dirty() noexcept {
        if (!scroll_dirty_valid_) return;
        mark_dirty_hint(scroll_dirty_accum_);
        scroll_dirty_valid_ = false;
        scroll_dirty_accum_ = {};
    }

    void mark_scroll_dirty(int old_scroll, int new_scroll) noexcept {
        const int dy = new_scroll - old_scroll;
        if (dy == 0) return;
        const auto clip = children_clip_rect();
        const auto band = alg::scroll::dirty_band_simple(to_alg_rect(clip), dy);
        const auto out = from_alg_rect(band);
        if (out.w > 0 && out.h > 0) {
            accumulate_scroll_dirty(out);
        }
    }

    void mark_scroll_dirty_inertia(int old_scroll, int new_scroll, int abs_v) noexcept {
        const int dy = new_scroll - old_scroll;
        if (dy == 0) return;
        const auto clip = children_clip_rect();
        const auto band = alg::scroll::dirty_band_inertia(to_alg_rect(clip),
                                                          dy,
                                                          abs_v,
                                                          inertia_fast_ratio_,
                                                          inertia_medium_ratio_,
                                                          inertia_extra_ratio_);
        const auto out = from_alg_rect(band);
        if (out.w > 0 && out.h > 0) {
            accumulate_scroll_dirty(out);
        }
    }

    static void on_pinch_begin(void* ctx) {
        auto* self = static_cast<ScrollContainer*>(ctx);
        if (!self) return;
        self->pinch_active_ = true;
        self->velocity_ = 0;
    }

    static void on_pinch_update(void* ctx, int dy) {
        auto* self = static_cast<ScrollContainer*>(ctx);
        if (!self) return;
        self->add_scroll_y(-dy);
        self->velocity_ = -dy;
    }

    static void on_pinch_end(void* ctx) {
        auto* self = static_cast<ScrollContainer*>(ctx);
        if (!self) return;
        self->pinch_active_ = false;
    }

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
    int clip_inset_left_{1};
    int clip_inset_top_{1};
    int clip_inset_right_{1};
    int clip_inset_bottom_{1};
    Rect scroll_dirty_accum_{};
    bool scroll_dirty_valid_{false};
    float inertia_fast_ratio_{0.5f};
    float inertia_medium_ratio_{0.25f};
    float inertia_extra_ratio_{0.125f};

    void update_clip_insets_for_skin() noexcept {
        if (!has_skin_) return;
        const int b = style_.border_width;
        clip_inset_left_ = (slice_left_ > b) ? slice_left_ : b;
        clip_inset_top_ = (slice_top_ > b) ? slice_top_ : b;
        clip_inset_right_ = (slice_right_ > b) ? slice_right_ : b;
        clip_inset_bottom_ = (slice_bottom_ > b) ? slice_bottom_ : b;
    }

    static float clamp_ratio(float v) noexcept {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }
};

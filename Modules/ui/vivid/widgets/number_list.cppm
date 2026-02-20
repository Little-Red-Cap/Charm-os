module;
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
export module charm.widgets.number_list;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.text;

using namespace ui::render;

// Number wheel list (ARM-2D number_list inspired)
export
class NumberList : public ObjectBase {
public:
    using ChangeFn = void(*)(void* ctx, int index, int value) noexcept;

    NumberList() {
        set_size(140, 160);
        set_focusable(true);
    }

    void set_item_count(int count) noexcept {
        item_count_ = (count > 0) ? count : 0;
        if (item_count_ == 0) {
            selected_ = 0;
            scroll_offset_ = 0.0f;
            target_scroll_ = 0.0f;
            return;
        }
        if (selected_ >= item_count_) selected_ = item_count_ - 1;
        snap_to_selected();
    }

    int item_count() const noexcept { return item_count_; }

    void set_range(int start, int delta) noexcept {
        start_ = start;
        delta_ = (delta != 0) ? delta : 1;
    }

    void set_format(const char* fmt) noexcept {
        format_ = (fmt && *fmt) ? fmt : "%d";
    }

    void set_item_height(int h) noexcept {
        item_h_ = (h > 8) ? h : 8;
        snap_to_selected();
    }

    int item_height() const noexcept { return item_h_; }

    void set_selected(int index) noexcept {
        if (item_count_ == 0) return;
        if (index < 0) index = 0;
        if (index >= item_count_) index = item_count_ - 1;
        if (selected_ == index && target_scroll_ == index * item_h_) return;
        selected_ = index;
        target_scroll_ = static_cast<float>(selected_ * item_h_);
        if (!smooth_scroll_) {
            scroll_offset_ = target_scroll_;
        }
        notify_change();
    }

    int selected() const noexcept { return selected_; }

    int value() const noexcept {
        return start_ + selected_ * delta_;
    }

    void set_on_change(ChangeFn fn, void* ctx = nullptr) noexcept {
        change_fn_ = fn;
        change_ctx_ = ctx;
    }

    void set_smooth_scroll(bool on) noexcept { smooth_scroll_ = on; }
    void set_wheel_step(int step) noexcept { wheel_step_ = (step > 0) ? step : 1; }

    void draw(CanvasBase& cvs) override {
        const Style& st = Theme::instance().get<NumberList>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        update_scroll_animation();

        const int pad = st.padding;
        const int center_y = r.y + r.h / 2;
        const int visible = (item_h_ > 0) ? (r.h / item_h_ + 4) : 0;
        int first = selected_ - visible / 2 - 2;
        int last = selected_ + visible / 2 + 2;
        if (first < 0) first = 0;
        if (last >= item_count_) last = item_count_ - 1;

        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);

        const Rect select_rect{r.x + pad, center_y - item_h_ / 2, r.w - pad * 2, item_h_};
        rgba select_bg = st.bg_pressed;
        select_bg.a = static_cast<std::uint8_t>(std::min(255, st.bg_pressed.a + 40));
        draw_round_rect(cvs, select_rect.x, select_rect.y, select_rect.w, select_rect.h,
                        st.corner_radius, select_bg, true);

        for (int i = first; i <= last; ++i) {
            const float item_center = center_y + (static_cast<float>(i * item_h_) - scroll_offset_);
            const float item_top = item_center - item_h_ / 2.0f;
            if (item_top + item_h_ < r.y || item_top > r.y + r.h) continue;

            const int draw_y = static_cast<int>(std::lround(item_top));
            Rect row{r.x + pad, draw_y, r.w - pad * 2, item_h_};

            const float dist = std::fabs(item_center - center_y);
            const float fade = 1.0f - std::min(dist / (r.h * 0.5f), 1.0f);
            rgba text_col = font;
            text_col.a = static_cast<std::uint8_t>(static_cast<int>(text_col.a * (0.35f + 0.65f * fade)));
            if (i == selected_) {
                text_col.a = font.a;
            }

            char buf[32]{};
            const int value = start_ + i * delta_;
            std::snprintf(buf, sizeof(buf), format_, value);

            draw_text_box(cvs, row, buf, text_col, resolve_font(st),
                          TextAlignH::Center, TextAlignV::Center, TextWrap::None, TextEllipsis::None);
        }

        cvs.restore_clip(clip_state);

        if (has_state(State::Focused)) {
            draw_rect(cvs, r.x, r.y, r.w, r.h, st.border_focus, false);
        }
    }

    bool on_event(const Event& e) override {
        const auto r = get_rect();
        if (e.type == Event::Type::MouseDown) {
            if (!r.contains(e.x, e.y)) return false;
            dragging_ = true;
            drag_start_y_ = e.y;
            drag_start_scroll_ = scroll_offset_;
            return true;
        }
        if (e.type == Event::Type::DragStart || e.type == Event::Type::DragMove) {
            if (!dragging_) return false;
            const int dy = (e.dy != 0) ? e.dy : (e.y - drag_start_y_);
            scroll_offset_ = drag_start_scroll_ - static_cast<float>(dy);
            clamp_scroll();
            update_selected_from_scroll();
            target_scroll_ = scroll_offset_;
            return true;
        }
        if (e.type == Event::Type::DragEnd || e.type == Event::Type::MouseUp) {
            if (!dragging_) return false;
            dragging_ = false;
            snap_to_selected();
            return true;
        }
        if (e.type == Event::Type::MouseWheel) {
            if (!r.contains(e.x, e.y)) return false;
            set_selected(selected_ - e.wheel_y * wheel_step_);
            return true;
        }
        if (e.type == Event::Type::Click) {
            if (!r.contains(e.x, e.y)) return false;
            const int index = index_from_y(e.y);
            set_selected(index);
            return true;
        }
        if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Up) {
                set_selected(selected_ - 1);
                return true;
            }
            if (e.key_code == Event::Key::Down) {
                set_selected(selected_ + 1);
                return true;
            }
        }
        return false;
    }

private:
    void clamp_scroll() noexcept {
        const float max_scroll = (item_count_ > 0) ? static_cast<float>((item_count_ - 1) * item_h_) : 0.0f;
        if (scroll_offset_ < 0.0f) scroll_offset_ = 0.0f;
        if (scroll_offset_ > max_scroll) scroll_offset_ = max_scroll;
    }

    void update_selected_from_scroll() noexcept {
        if (item_count_ == 0 || item_h_ <= 0) return;
        const int idx = static_cast<int>(std::lround(scroll_offset_ / static_cast<float>(item_h_)));
        const int clamped = (idx < 0) ? 0 : (idx >= item_count_ ? item_count_ - 1 : idx);
        if (clamped != selected_) {
            selected_ = clamped;
            notify_change();
        }
    }

    void update_scroll_animation() noexcept {
        if (!smooth_scroll_ || dragging_) return;
        const float diff = target_scroll_ - scroll_offset_;
        if (std::fabs(diff) < 0.5f) {
            scroll_offset_ = target_scroll_;
            return;
        }
        scroll_offset_ += diff * 0.25f;
        clamp_scroll();
    }

    void snap_to_selected() noexcept {
        if (item_count_ == 0) return;
        target_scroll_ = static_cast<float>(selected_ * item_h_);
        if (!smooth_scroll_) {
            scroll_offset_ = target_scroll_;
        }
    }

    int index_from_y(int y) const noexcept {
        if (item_count_ == 0 || item_h_ <= 0) return 0;
        const auto r = get_rect();
        const int center_y = r.y + r.h / 2;
        const float pos = scroll_offset_ + static_cast<float>(y - center_y);
        int idx = static_cast<int>(std::lround(pos / static_cast<float>(item_h_)));
        if (idx < 0) idx = 0;
        if (idx >= item_count_) idx = item_count_ - 1;
        return idx;
    }

    void notify_change() noexcept {
        if (!change_fn_) return;
        change_fn_(change_ctx_, selected_, value());
    }

    int item_count_{0};
    int item_h_{28};
    int selected_{0};
    int start_{0};
    int delta_{1};
    const char* format_{"%d"};
    int wheel_step_{1};
    bool smooth_scroll_{true};

    bool dragging_{false};
    int drag_start_y_{0};
    float drag_start_scroll_{0.0f};
    float scroll_offset_{0.0f};
    float target_scroll_{0.0f};

    ChangeFn change_fn_{nullptr};
    void* change_ctx_{nullptr};
};

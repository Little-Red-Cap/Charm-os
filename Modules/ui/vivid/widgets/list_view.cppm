module;
#include <algorithm>
export module charm.widgets.list_view;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;

using namespace ui::render;

export
class ListView : public ObjectBase {
public:
    struct DrawInfo {
        Rect rect{};
        int index{0};
        bool selected{false};
    };

    using DrawRowFn = void(*)(void* ctx, DefaultCanvas& cvs, const DrawInfo& info) noexcept;
    using SelectFn = void(*)(void* ctx, int index) noexcept;

    ListView() {
        set_size(240, 180);
        set_focusable(true);
    }

    void set_item_count(int count) noexcept {
        item_count_ = (count > 0) ? count : 0;
        if (selected_ >= item_count_) selected_ = item_count_ - 1;
        update_scroll_bounds();
    }

    int item_count() const noexcept { return item_count_; }

    void set_row_height(int h) noexcept {
        row_height_ = (h > 4) ? h : 4;
        update_scroll_bounds();
    }

    int row_height() const noexcept { return row_height_; }

    void set_on_draw(DrawRowFn fn, void* ctx = nullptr) noexcept {
        draw_fn_ = fn;
        draw_ctx_ = ctx;
    }

    void set_on_select(SelectFn fn, void* ctx = nullptr) noexcept {
        select_fn_ = fn;
        select_ctx_ = ctx;
    }

    void set_selected(int index) noexcept {
        if (index < 0 || index >= item_count_) return;
        selected_ = index;
        ensure_visible(index);
        if (select_fn_) select_fn_(select_ctx_, index);
    }

    int selected() const noexcept { return selected_; }

    void set_scroll_y(int y) noexcept {
        scroll_y_ = clamp_scroll(y);
    }

    void add_scroll_y(int dy) noexcept {
        scroll_y_ = clamp_scroll(scroll_y_ + dy);
    }

    void set_wheel_step(int step) noexcept { wheel_step_ = step; }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<ListView>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        update_scroll_bounds();

        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);

        const int pad = st.padding;
        const int content_x = r.x + pad;
        const int content_w = r.w - pad * 2;
        const int start = (row_height_ > 0) ? (scroll_y_ / row_height_) : 0;
        const int offset_y = r.y + pad - (scroll_y_ % row_height_);

        int y = offset_y;
        for (int i = start; i < item_count_ && y < r.y + r.h; ++i) {
            Rect row{content_x, y, content_w, row_height_};
            const bool is_selected = (i == selected_);
            if (is_selected) {
                draw_rect(cvs, row.x, row.y, row.w, row.h, st.bg_pressed, true);
            }
            if (draw_fn_) {
                draw_fn_(draw_ctx_, cvs, DrawInfo{row, i, is_selected});
            }
            y += row_height_;
        }

        cvs.restore_clip(clip_state);

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

        if (has_state(State::Focused)) {
            draw_rect(cvs, r.x, r.y, r.w, r.h, st.border_focus, false);
        }
    }

    bool on_event(const Event& e) override {
        const auto r = get_rect();
        if (e.type == Event::Type::MouseDown) {
            if (!r.contains(e.x, e.y)) return false;
            dragging_ = true;
            last_y_ = e.y;
            return true;
        } else if (e.type == Event::Type::DragStart || e.type == Event::Type::DragMove) {
            if (dragging_) {
                const int dy = (e.dy != 0) ? e.dy : (e.y - last_y_);
                last_y_ = e.y;
                add_scroll_y(-dy);
                return true;
            }
        } else if (e.type == Event::Type::DragEnd || e.type == Event::Type::MouseUp) {
            dragging_ = false;
            return true;
        } else if (e.type == Event::Type::MouseWheel) {
            if (!r.contains(e.x, e.y)) return false;
            const int target_step = e.wheel_y * wheel_step_;
            add_scroll_y(-target_step);
            return true;
        } else if (e.type == Event::Type::Click) {
            if (!r.contains(e.x, e.y)) return false;
            const int index = index_from_y(e.y);
            if (index >= 0 && index < item_count_) {
                set_selected(index);
                return true;
            }
        } else if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Up) {
                if (selected_ > 0) set_selected(selected_ - 1);
                return true;
            }
            if (e.key_code == Event::Key::Down) {
                if (selected_ + 1 < item_count_) set_selected(selected_ + 1);
                return true;
            }
        }
        return false;
    }

private:
    void update_scroll_bounds() noexcept {
        const auto r = get_rect();
        const Style& st = Theme::instance().get<ListView>();
        content_height_ = item_count_ * row_height_ + st.padding * 2;
        const int max = content_height_ - r.h;
        max_scroll_ = (max > 0) ? max : 0;
        if (scroll_y_ > max_scroll_) scroll_y_ = max_scroll_;
        if (scroll_y_ < 0) scroll_y_ = 0;
    }

    int clamp_scroll(int y) const noexcept {
        if (y < 0) return 0;
        if (y > max_scroll_) return max_scroll_;
        return y;
    }

    void ensure_visible(int index) noexcept {
        if (index < 0) return;
        const Style& st = Theme::instance().get<ListView>();
        const auto r = get_rect();
        const int pad = st.padding;
        const int row_top = index * row_height_;
        const int row_bottom = row_top + row_height_;
        const int view_top = scroll_y_;
        const int view_bottom = scroll_y_ + (r.h - pad * 2);
        if (row_top < view_top) {
            set_scroll_y(row_top);
        } else if (row_bottom > view_bottom) {
            set_scroll_y(row_bottom - (r.h - pad * 2));
        }
    }

    int index_from_y(int y) const noexcept {
        const Style& st = Theme::instance().get<ListView>();
        const auto r = get_rect();
        const int local = y - r.y + scroll_y_ - st.padding;
        if (local < 0) return -1;
        if (row_height_ <= 0) return -1;
        return local / row_height_;
    }

    DrawRowFn draw_fn_{nullptr};
    void* draw_ctx_{nullptr};
    SelectFn select_fn_{nullptr};
    void* select_ctx_{nullptr};

    int item_count_{0};
    int row_height_{24};
    int selected_{-1};
    int scroll_y_{0};
    int max_scroll_{0};
    int content_height_{0};
    int wheel_step_{24};
    bool dragging_{false};
    int last_y_{0};
};

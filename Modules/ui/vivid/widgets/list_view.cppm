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
        int slot{-1};
    };

    using CountFn = int(*)(void* ctx) noexcept;
    using DrawRowFn = void(*)(void* ctx, DefaultCanvas& cvs, const DrawInfo& info) noexcept;
    using SelectFn = void(*)(void* ctx, int index) noexcept;
    using CacheFn = void(*)(void* ctx, int slot, int index) noexcept;
    using PoolCreateFn = void(*)(void* ctx, int slot) noexcept;
    using PoolBindFn = void(*)(void* ctx, int slot, int index) noexcept;
    using PoolRecycleFn = void(*)(void* ctx, int slot, int index) noexcept;
    using ScrollFn = void(*)(void* ctx, int scroll_y, int max_scroll, int view_h, int content_h) noexcept;

    ListView() {
        set_size(240, 180);
        set_focusable(true);
    }

    void set_item_count(int count) noexcept {
        item_count_ = (count > 0) ? count : 0;
        if (selected_ >= item_count_) selected_ = item_count_ - 1;
        clear_cache();
        update_scroll_bounds();
    }

    int item_count() const noexcept { return item_count_; }

    void set_data_source(CountFn count_fn, DrawRowFn draw_fn, void* ctx = nullptr) noexcept {
        count_fn_ = count_fn;
        draw_fn_ = draw_fn;
        data_ctx_ = ctx;
        clear_cache();
        update_scroll_bounds();
    }

    void set_row_height(int h) noexcept {
        row_height_ = (h > 4) ? h : 4;
        clear_cache();
        update_scroll_bounds();
    }

    int row_height() const noexcept { return row_height_; }

    void set_on_draw(DrawRowFn fn, void* ctx = nullptr) noexcept {
        draw_fn_ = fn;
        draw_ctx_ = ctx;
    }

    void set_cache_handler(CacheFn fn, void* ctx = nullptr) noexcept {
        cache_fn_ = fn;
        cache_ctx_ = ctx;
        clear_cache();
    }

    void set_item_pool(PoolCreateFn create_fn,
                       PoolBindFn bind_fn,
                       PoolRecycleFn recycle_fn,
                       void* ctx = nullptr) noexcept {
        pool_create_fn_ = create_fn;
        pool_bind_fn_ = bind_fn;
        pool_recycle_fn_ = recycle_fn;
        pool_ctx_ = ctx;
        clear_cache();
    }

    void set_on_scroll(ScrollFn fn, void* ctx = nullptr) noexcept {
        scroll_fn_ = fn;
        scroll_ctx_ = ctx;
        notify_scroll();
    }

    void set_on_select(SelectFn fn, void* ctx = nullptr) noexcept {
        select_fn_ = fn;
        select_ctx_ = ctx;
    }

    void set_prefetch_rows(int rows) noexcept {
        prefetch_rows_ = (rows > 0) ? rows : 0;
        clear_cache();
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
        notify_scroll();
    }

    void add_scroll_y(int dy) noexcept {
        scroll_y_ = clamp_scroll(scroll_y_ + dy);
        notify_scroll();
    }

    void set_wheel_step(int step) noexcept { wheel_step_ = step; }
    void set_show_scrollbar(bool on) noexcept { show_scrollbar_ = on; }

    int scroll_y() const noexcept { return scroll_y_; }
    int max_scroll() const noexcept { return max_scroll_; }
    int content_height() const noexcept { return content_height_; }

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
        const int count = item_count_for_render();
        const int row_h = row_height_for_render();
        int start = (row_h > 0) ? (scroll_y_ / row_h) : 0;
        if (start > 0 && prefetch_rows_ > 0) {
            start = (start > prefetch_rows_) ? (start - prefetch_rows_) : 0;
        }
        const int offset_y = r.y + pad - (scroll_y_ - start * row_h);

        int visible = (row_h > 0) ? ((r.h + row_h - 1) / row_h + 1) : 0;
        if (prefetch_rows_ > 0) visible += prefetch_rows_ * 2;
        int y = offset_y;
        for (int i = start; i < count && y < r.y + r.h; ++i) {
            Rect row{content_x, y, content_w, row_h};
            const bool is_selected = (i == selected_);
            if (is_selected) {
                draw_rect(cvs, row.x, row.y, row.w, row.h, st.bg_pressed, true);
            }
            int slot = -1;
            if (visible > 0 && visible <= kMaxCache) {
                slot = i - start;
                if (slot >= 0 && slot < kMaxCache) {
                    touch_slot(slot);
                    if (cache_slots_[slot].index != i) {
                        recycle_slot(slot);
                        cache_slots_[slot].index = i;
                        if (pool_create_fn_ && !cache_slots_[slot].created) {
                            pool_create_fn_(pool_ctx_, slot);
                            cache_slots_[slot].created = true;
                        }
                        if (pool_bind_fn_) {
                            pool_bind_fn_(pool_ctx_, slot, i);
                        } else if (cache_fn_) {
                            cache_fn_(cache_ctx_, slot, i);
                        }
                    }
                } else {
                    slot = -1;
                }
            }
            if (draw_fn_) {
                const void* ctx = data_ctx_ ? data_ctx_ : draw_ctx_;
                draw_fn_(const_cast<void*>(ctx), cvs, DrawInfo{row, i, is_selected, slot});
            }
            y += row_h;
        }

        recycle_inactive_slots();

        cvs.restore_clip(clip_state);

        if (show_scrollbar_ && content_height_ > r.h) {
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
            const int count = item_count_for_render();
            if (index >= 0 && index < count) {
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
        const int count = item_count_for_render();
        const int row_h = row_height_for_render();
        content_height_ = count * row_h + st.padding * 2;
        const int max = content_height_ - r.h;
        max_scroll_ = (max > 0) ? max : 0;
        if (scroll_y_ > max_scroll_) scroll_y_ = max_scroll_;
        if (scroll_y_ < 0) scroll_y_ = 0;
        notify_scroll();
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
        const int row_h = row_height_for_render();
        const int row_top = index * row_h;
        const int row_bottom = row_top + row_h;
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
        const int row_h = row_height_for_render();
        if (row_h <= 0) return -1;
        return local / row_h;
    }

    int item_count_for_render() const noexcept {
        if (count_fn_) {
            const int count = count_fn_(data_ctx_);
            return (count > 0) ? count : 0;
        }
        return item_count_;
    }

    int row_height_for_render() const noexcept {
        return (row_height_ > 4) ? row_height_ : 4;
    }

    void notify_scroll() noexcept {
        if (!scroll_fn_) return;
        const auto r = get_rect();
        scroll_fn_(scroll_ctx_, scroll_y_, max_scroll_, r.h, content_height_);
    }

    void clear_cache() noexcept {
        for (int i = 0; i < kMaxCache; ++i) {
            recycle_slot(i);
            cache_slots_[i].index = -1;
            cache_slots_[i].touched = false;
            cache_slots_[i].created = false;
        }
    }

    void touch_slot(int slot) noexcept {
        if (slot < 0 || slot >= kMaxCache) return;
        cache_slots_[slot].touched = true;
    }

    void recycle_slot(int slot) noexcept {
        if (slot < 0 || slot >= kMaxCache) return;
        if (cache_slots_[slot].index >= 0 && pool_recycle_fn_) {
            pool_recycle_fn_(pool_ctx_, slot, cache_slots_[slot].index);
        }
    }

    void recycle_inactive_slots() noexcept {
        for (int i = 0; i < kMaxCache; ++i) {
            if (!cache_slots_[i].touched && cache_slots_[i].index >= 0) {
                recycle_slot(i);
                cache_slots_[i].index = -1;
            }
            cache_slots_[i].touched = false;
        }
    }

    CountFn count_fn_{nullptr};
    DrawRowFn draw_fn_{nullptr};
    void* draw_ctx_{nullptr};
    void* data_ctx_{nullptr};
    SelectFn select_fn_{nullptr};
    void* select_ctx_{nullptr};
    CacheFn cache_fn_{nullptr};
    void* cache_ctx_{nullptr};
    PoolCreateFn pool_create_fn_{nullptr};
    PoolBindFn pool_bind_fn_{nullptr};
    PoolRecycleFn pool_recycle_fn_{nullptr};
    void* pool_ctx_{nullptr};
    ScrollFn scroll_fn_{nullptr};
    void* scroll_ctx_{nullptr};

    int item_count_{0};
    int row_height_{24};
    int selected_{-1};
    int scroll_y_{0};
    int max_scroll_{0};
    int content_height_{0};
    int wheel_step_{24};
    bool dragging_{false};
    int last_y_{0};
    bool show_scrollbar_{true};
    int prefetch_rows_{1};

    struct CacheSlot {
        int index{-1};
        bool touched{false};
        bool created{false};
    };
    static constexpr int kMaxCache = 32;
    CacheSlot cache_slots_[kMaxCache]{};
};

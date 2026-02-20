module;
#include <algorithm>
export module charm.widgets.list_view;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.core.virtual_list;
import alg_list_layout;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;

using namespace ui::render;

export
class ListView : public ObjectBase {
public:
    static constexpr int kLayoutId = 2;

    struct DrawInfo {
        Rect rect{};
        int index{0};
        bool selected{false};
        int slot{-1};
    };

    using CountFn = int(*)(void* ctx) noexcept;
    using DrawRowFn = void(*)(void* ctx, CanvasBase& cvs, const DrawInfo& info) noexcept;
    using SelectFn = void(*)(void* ctx, int index) noexcept;
    using CacheFn = void(*)(void* ctx, int slot, int index) noexcept;
    using PoolCreateFn = void(*)(void* ctx, int slot) noexcept;
    using PoolBindFn = void(*)(void* ctx, int slot, int index) noexcept;
    using PoolRecycleFn = void(*)(void* ctx, int slot, int index) noexcept;
    using RowHeightFn = int(*)(void* ctx, int index) noexcept;
    using ScrollFn = void(*)(void* ctx, int scroll_y, int max_scroll, int view_h, int content_h) noexcept;

    ListView() {
        set_size(240, 180);
        set_focusable(true);
        set_custom_layout(kLayoutId);
    }

    void set_item_count(int count) noexcept {
        item_count_ = (count > 0) ? count : 0;
        if (selected_ >= item_count_) selected_ = item_count_ - 1;
        clear_cache();
        update_scroll_bounds();
        window_valid_ = false;
        mark_dirty_hint(get_rect());
    }

    int item_count() const noexcept { return item_count_; }

    void set_data_source(CountFn count_fn, DrawRowFn draw_fn, void* ctx = nullptr) noexcept {
        count_fn_ = count_fn;
        draw_fn_ = draw_fn;
        data_ctx_ = ctx;
        clear_cache();
        update_scroll_bounds();
        window_valid_ = false;
        mark_dirty_hint(get_rect());
    }

    void set_row_height(int h) noexcept {
        row_height_ = (h > 4) ? h : 4;
        row_height_fn_ = nullptr;
        row_height_ctx_ = nullptr;
        invalidate_cache_from(0);
        update_scroll_bounds();
        window_valid_ = false;
        mark_dirty_hint(get_rect());
    }

    int row_height() const noexcept { return row_height_; }

    void set_row_height_fn(RowHeightFn fn, void* ctx = nullptr) noexcept {
        row_height_fn_ = fn;
        row_height_ctx_ = ctx;
        invalidate_cache_from(0);
        update_scroll_bounds();
        window_valid_ = false;
        mark_dirty_hint(get_rect());
    }

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
        window_valid_ = false;
        mark_dirty_hint(get_rect());
    }

    void notify_rows_changed(int start, int count) noexcept {
        const int total = item_count_for_render();
        if (total <= 0) return;
        int range_start = start;
        int range_end = start + count;
        if (range_start < 0) range_start = 0;
        if (range_end > total) range_end = total;
        if (range_start >= range_end) return;
        invalidate_cache_range(range_start, range_end);
        mark_dirty_rows_range(range_start, range_end);
    }

    void notify_row_height_changed(int start, int count) noexcept {
        const int total = item_count_for_render();
        if (total <= 0) return;
        int range_start = start;
        int range_end = start + count;
        if (range_start < 0) range_start = 0;
        if (range_end > total) range_end = total;
        if (range_start >= range_end) return;
        invalidate_cache_from(range_start);
        update_scroll_bounds();
        window_valid_ = false;
        mark_dirty_from_row(range_start);
    }

    void set_selected(int index) noexcept {
        if (index < 0 || index >= item_count_) return;
        const int prev = selected_;
        selected_ = index;
        ensure_visible(index);
        mark_dirty_row(prev);
        mark_dirty_row(index);
        if (select_fn_) select_fn_(select_ctx_, index);
    }

    int selected() const noexcept { return selected_; }

    void set_scroll_y(int y) noexcept {
        const int old = scroll_y_;
        scroll_y_ = clamp_scroll(y);
        notify_scroll();
        window_valid_ = false;
        mark_scroll_dirty(old, scroll_y_);
    }

    void add_scroll_y(int dy) noexcept {
        const int old = scroll_y_;
        scroll_y_ = clamp_scroll(scroll_y_ + dy);
        notify_scroll();
        window_valid_ = false;
        mark_scroll_dirty(old, scroll_y_);
    }

    void set_wheel_step(int step) noexcept { wheel_step_ = step; }
    void set_show_scrollbar(bool on) noexcept { show_scrollbar_ = on; }

    int scroll_y() const noexcept { return scroll_y_; }
    int max_scroll() const noexcept { return max_scroll_; }
    int content_height() const noexcept { return content_height_; }

    void draw(CanvasBase& cvs) override {
        const Style& st = Theme::instance().get<ListView>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        flush_scroll_dirty();

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);

        const int pad = st.padding;
        const int content_x = r.x + pad;
        const int content_w = r.w - pad * 2;
        const int count = item_count_for_render();
        if (!window_valid_) update_visible_window();
        int start = window_start_;
        int visible = window_visible_;
        int y = window_offset_y_;
        const bool variable_height = (row_height_fn_ != nullptr);
        auto on_create = [&](int slot) {
            if (pool_create_fn_) pool_create_fn_(pool_ctx_, slot);
        };
        auto on_recycle = [&](int slot, int index) {
            if (pool_recycle_fn_) pool_recycle_fn_(pool_ctx_, slot, index);
        };
        auto on_bind = [&](int slot, int index) {
            if (pool_bind_fn_) pool_bind_fn_(pool_ctx_, slot, index);
            else if (cache_fn_) cache_fn_(cache_ctx_, slot, index);
        };
        cache_.begin_frame();
        for (int i = start; i < count && y < r.y + r.h; ++i) {
            const int row_h = variable_height ? row_height_for_index(i) : row_height_for_render();
            Rect row{content_x, y, content_w, row_h};
            const bool is_selected = (i == selected_);
            if (is_selected) {
                draw_rect(cvs, row.x, row.y, row.w, row.h, st.bg_pressed, true);
            }
            int slot = -1;
            if (visible > 0 && visible <= kMaxCache) {
                slot = i - start;
                if (slot >= 0 && slot < kMaxCache) {
                    cache_.bind_slot(slot, i, on_create, on_recycle, on_bind);
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

        cache_.recycle_inactive(on_recycle);

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

    void update_visible_window() noexcept {
        const Style& st = Theme::instance().get<ListView>();
        const auto r = get_rect();
        update_scroll_bounds();
        const int pad = st.padding;
        const int count = item_count_for_render();
        const bool variable_height = (row_height_fn_ != nullptr);
        int start = 0;
        int visible = 0;
        int y = r.y + pad;
        if (!variable_height) {
            const int row_h = row_height_for_render();
            const int view_h = r.h - pad * 2;
            auto layout = alg::list::derive_layout(
                static_cast<std::int16_t>(view_h),
                static_cast<std::int16_t>(row_h),
                0,
                static_cast<std::int16_t>(count),
                static_cast<std::int16_t>(scroll_y_));
            start = layout.top_index;
            visible = layout.row_count;
            y = r.y + pad + layout.row_offset;
            if (prefetch_rows_ > 0 && count > 0) {
                int pref = prefetch_rows_;
                int pref_start = start - pref;
                if (pref_start < 0) pref_start = 0;
                const int actual_pref = start - pref_start;
                start = pref_start;
                y -= actual_pref * row_h;
                visible += actual_pref;
                int extra = pref;
                const int max_extra = count - (start + visible);
                if (extra > max_extra) extra = max_extra;
                if (extra > 0) visible += extra;
            }
            if (visible < 0) visible = 0;
            if (start < 0) start = 0;
            if (start + visible > count) visible = count - start;
        } else {
            int acc = 0;
            for (int i = 0; i < count; ++i) {
                const int h = row_height_for_index(i);
                if (acc + h > scroll_y_) {
                    start = i;
                    break;
                }
                acc += h;
                start = i + 1;
            }
            if (prefetch_rows_ > 0) {
                for (int p = 0; p < prefetch_rows_ && start > 0; ++p) {
                    --start;
                    acc -= row_height_for_index(start);
                }
            }
            y = r.y + pad - (scroll_y_ - acc);
            int temp_y = y;
            for (int i = start; i < count && temp_y < r.y + r.h; ++i) {
                temp_y += row_height_for_index(i);
                ++visible;
            }
        }
        window_start_ = start;
        window_visible_ = visible;
        window_offset_y_ = y;
        window_valid_ = true;
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
        } else if (e.type == Event::Type::GestureSwipe) {
            if (!r.contains(e.x, e.y)) return false;
            if (e.gesture_phase == Event::GesturePhase::Begin) {
                swipe_active_ = true;
            } else if (e.gesture_phase == Event::GesturePhase::Update) {
                if (swipe_active_) {
                    add_scroll_y(-e.dy);
                }
            } else if (e.gesture_phase == Event::GesturePhase::End) {
                swipe_active_ = false;
            }
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
    static Rect intersect_rect(const Rect& a, const Rect& b) noexcept {
        const int left = (a.x > b.x) ? a.x : b.x;
        const int top = (a.y > b.y) ? a.y : b.y;
        const int right = ((a.x + a.w) < (b.x + b.w)) ? (a.x + a.w) : (b.x + b.w);
        const int bottom = ((a.y + a.h) < (b.y + b.h)) ? (a.y + a.h) : (b.y + b.h);
        const int w = right - left;
        const int h = bottom - top;
        if (w <= 0 || h <= 0) return {};
        return Rect{left, top, w, h};
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
        const auto r = get_rect();
        if (dy > r.h || dy < -r.h) {
            accumulate_scroll_dirty(r);
            return;
        }
        if (dy > r.h / 2 || dy < -r.h / 2) {
            accumulate_scroll_dirty(r);
            return;
        }
        Rect band{};
        if (dy > 0) {
            band = Rect{r.x, r.y + r.h - dy, r.w, dy};
        } else {
            band = Rect{r.x, r.y, r.w, -dy};
        }
        const auto clipped = intersect_rect(band, r);
        if (clipped.w > 0 && clipped.h > 0) {
            accumulate_scroll_dirty(clipped);
        } else {
            accumulate_scroll_dirty(r);
        }
    }

    void mark_dirty_row(int index) noexcept {
        if (index < 0) return;
        const auto row = row_rect_for_index(index);
        if (row.w <= 0 || row.h <= 0) return;
        const auto clipped = intersect_rect(row, get_rect());
        if (clipped.w <= 0 || clipped.h <= 0) return;
        mark_dirty_hint(clipped);
    }

    void mark_dirty_rows_range(int start, int end) noexcept {
        const auto range = row_range_rect(start, end);
        if (range.w <= 0 || range.h <= 0) return;
        const auto clipped = intersect_rect(range, get_rect());
        if (clipped.w <= 0 || clipped.h <= 0) return;
        mark_dirty_hint(clipped);
    }

    void mark_dirty_from_row(int start) noexcept {
        const int end = item_count_for_render();
        mark_dirty_rows_range(start, end);
    }

    Rect row_rect_for_index(int index) const noexcept {
        const Style& st = Theme::instance().get<ListView>();
        const auto r = get_rect();
        const int pad = st.padding;
        int row_top = 0;
        int row_h = row_height_for_index(index);
        if (row_height_fn_) {
            row_top = offset_for_index(index);
        } else {
            row_top = index * row_height_for_render();
        }
        return Rect{
            r.x + pad,
            r.y + pad + row_top - scroll_y_,
            r.w - pad * 2,
            row_h
        };
    }

    Rect row_range_rect(int start, int end) const noexcept {
        const Style& st = Theme::instance().get<ListView>();
        const auto r = get_rect();
        if (start < 0) start = 0;
        if (end < start) end = start;
        const int count = item_count_for_render();
        if (end > count) end = count;
        if (start >= end) return {};
        const int pad = st.padding;
        int row_top = 0;
        int range_h = 0;
        if (row_height_fn_) {
            row_top = offset_for_index(start);
            for (int i = start; i < end; ++i) {
                range_h += row_height_for_index(i);
            }
        } else {
            const int row_h = row_height_for_render();
            row_top = start * row_h;
            range_h = (end - start) * row_h;
        }
        return Rect{
            r.x + pad,
            r.y + pad + row_top - scroll_y_,
            r.w - pad * 2,
            range_h
        };
    }

    void update_scroll_bounds() noexcept {
        const auto r = get_rect();
        const Style& st = Theme::instance().get<ListView>();
        const int count = item_count_for_render();
        if (row_height_fn_) {
            int sum = 0;
            for (int i = 0; i < count; ++i) {
                sum += row_height_for_index(i);
            }
            content_height_ = sum + st.padding * 2;
        } else {
            const int row_h = row_height_for_render();
            content_height_ = count * row_h + st.padding * 2;
        }
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
        int row_top = 0;
        int row_bottom = 0;
        if (row_height_fn_) {
            row_top = offset_for_index(index);
            row_bottom = row_top + row_height_for_index(index);
        } else {
            const int row_h = row_height_for_render();
            row_top = index * row_h;
            row_bottom = row_top + row_h;
        }
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
        if (!row_height_fn_) {
            const int row_h = row_height_for_render();
            if (row_h <= 0) return -1;
            return local / row_h;
        }
        int acc = 0;
        const int count = item_count_for_render();
        for (int i = 0; i < count; ++i) {
            acc += row_height_for_index(i);
            if (local < acc) return i;
        }
        return -1;
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

    int row_height_for_index(int index) const noexcept {
        if (!row_height_fn_) return row_height_for_render();
        int h = row_height_fn_(row_height_ctx_ ? row_height_ctx_ : data_ctx_, index);
        return (h > 4) ? h : 4;
    }

    int offset_for_index(int index) const noexcept {
        int acc = 0;
        for (int i = 0; i < index; ++i) {
            acc += row_height_for_index(i);
        }
        return acc;
    }

    void notify_scroll() noexcept {
        if (!scroll_fn_) return;
        const auto r = get_rect();
        scroll_fn_(scroll_ctx_, scroll_y_, max_scroll_, r.h, content_height_);
    }

    void clear_cache() noexcept {
        cache_.clear([&](int slot, int index) {
            if (pool_recycle_fn_) pool_recycle_fn_(pool_ctx_, slot, index);
        });
    }

    void invalidate_cache_range(int start, int end) noexcept {
        cache_.recycle_if([&](int slot, int index) {
            if (pool_recycle_fn_) pool_recycle_fn_(pool_ctx_, slot, index);
        }, [&](int index) {
            return index >= start && index < end;
        });
    }

    void invalidate_cache_from(int start) noexcept {
        cache_.recycle_if([&](int slot, int index) {
            if (pool_recycle_fn_) pool_recycle_fn_(pool_ctx_, slot, index);
        }, [&](int index) {
            return index >= start;
        });
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
    RowHeightFn row_height_fn_{nullptr};
    void* row_height_ctx_{nullptr};
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
    bool swipe_active_{false};
    int window_start_{0};
    int window_visible_{0};
    int window_offset_y_{0};
    bool window_valid_{false};
    Rect scroll_dirty_accum_{};
    bool scroll_dirty_valid_{false};

    static constexpr int kMaxCache = 32;
    VirtualListCache<kMaxCache> cache_{};
};

module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
export module charm.widgets.list_view;

import charm.core.object;
import charm.core.event;
import charm.core.geometry;
import charm.core.structured_view;
import charm.core.virtual_list;
import alg_scroll_bounds;
import alg_scroll_thumb;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.style;
import charm.core.style_sheet;

using namespace ui::render;

export
class ListView : public WidgetBase<ListView> {
public:
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
    using RowFlagsFn = StructuredListRowFlagsFn;
    using ScrollFn = void(*)(void* ctx, int scroll_y, int max_scroll, int view_h, int content_h) noexcept;

    class ItemPoolWorkspace {
    public:
        static constexpr std::size_t capacity = 32;

        ItemPoolWorkspace() = default;
        ~ItemPoolWorkspace() noexcept;
        ItemPoolWorkspace(const ItemPoolWorkspace&) = delete;
        ItemPoolWorkspace& operator=(const ItemPoolWorkspace&) = delete;
        ItemPoolWorkspace(ItemPoolWorkspace&&) = delete;
        ItemPoolWorkspace& operator=(ItemPoolWorkspace&&) = delete;

        void set_cache_handler(CacheFn fn, void* ctx = nullptr) noexcept {
            recycle_cache();
            cache_fn_ = fn;
            cache_ctx_ = ctx;
            pool_create_fn_ = nullptr;
            pool_bind_fn_ = nullptr;
            pool_recycle_fn_ = nullptr;
            pool_ctx_ = nullptr;
        }

        void set_item_pool(PoolCreateFn create_fn,
                           PoolBindFn bind_fn,
                           PoolRecycleFn recycle_fn,
                           void* ctx = nullptr) noexcept {
            recycle_cache();
            cache_fn_ = nullptr;
            cache_ctx_ = nullptr;
            pool_create_fn_ = create_fn;
            pool_bind_fn_ = bind_fn;
            pool_recycle_fn_ = recycle_fn;
            pool_ctx_ = ctx;
        }

        void set_prefetch_rows(int rows) noexcept {
            prefetch_rows_ = (rows > 0) ? rows : 0;
        }

        void reset() noexcept {
            recycle_cache();
            cache_fn_ = nullptr;
            cache_ctx_ = nullptr;
            pool_create_fn_ = nullptr;
            pool_bind_fn_ = nullptr;
            pool_recycle_fn_ = nullptr;
            pool_ctx_ = nullptr;
            prefetch_rows_ = 1;
        }

    private:
        friend class ListView;

        void recycle_cache() noexcept {
            cache_.clear([&](int slot, int index) {
                if (pool_recycle_fn_) pool_recycle_fn_(pool_ctx_, slot, index);
            });
        }

        VirtualListCache<capacity> cache_{};
        CacheFn cache_fn_{nullptr};
        void* cache_ctx_{nullptr};
        PoolCreateFn pool_create_fn_{nullptr};
        PoolBindFn pool_bind_fn_{nullptr};
        PoolRecycleFn pool_recycle_fn_{nullptr};
        void* pool_ctx_{nullptr};
        int prefetch_rows_{1};
        ListView* owner_{nullptr};
    };

    ListView() {
        set_size(240, 180);
        set_focusable(true);
    }

    ~ListView() noexcept {
        detach_item_pool_workspace();
    }

    ListView(const ListView&) = delete;
    ListView& operator=(const ListView&) = delete;
    ListView(ListView&&) = delete;
    ListView& operator=(ListView&&) = delete;

    void set_item_count(int count) noexcept {
        item_count_ = (count > 0) ? count : 0;
        if (selected_ >= item_count_) selected_ = item_count_ - 1;
        clear_cache();
        update_scroll_bounds();
        window_valid_ = false;
    }

    int item_count() const noexcept { return item_count_; }

    void set_data_source(CountFn count_fn, DrawRowFn draw_fn, void* ctx = nullptr) noexcept {
        count_fn_ = count_fn;
        draw_fn_ = draw_fn;
        data_ctx_ = ctx;
        draw_ctx_ = ctx;
        clear_cache();
        update_scroll_bounds();
        window_valid_ = false;
    }

    void set_row_height(int h) noexcept {
        row_height_ = (h > 4) ? h : 4;
        row_height_fn_ = nullptr;
        row_height_ctx_ = nullptr;
        invalidate_cache_from(0);
        update_scroll_bounds();
        window_valid_ = false;
    }

    int row_height() const noexcept { return row_height_; }

    void set_row_height_fn(RowHeightFn fn, void* ctx = nullptr) noexcept {
        row_height_fn_ = fn;
        row_height_ctx_ = ctx;
        invalidate_cache_from(0);
        update_scroll_bounds();
        window_valid_ = false;
    }

    void set_row_flags_fn(RowFlagsFn fn, void* ctx = nullptr) noexcept {
        row_flags_fn_ = fn;
        row_flags_ctx_ = ctx;
    }

    void set_on_draw(DrawRowFn fn, void* ctx = nullptr) noexcept {
        draw_fn_ = fn;
        draw_ctx_ = ctx;
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

    [[nodiscard]] bool attach_item_pool_workspace(ItemPoolWorkspace& workspace) noexcept {
        if (workspace.owner_ != nullptr && workspace.owner_ != this) return false;
        if (item_pool_workspace_ == &workspace) return true;
        detach_item_pool_workspace();
        item_pool_workspace_ = &workspace;
        workspace.owner_ = this;
        window_valid_ = false;
        return true;
    }

    void detach_item_pool_workspace() noexcept {
        if (!item_pool_workspace_) return;
        clear_cache();
        if (item_pool_workspace_->owner_ == this) item_pool_workspace_->owner_ = nullptr;
        item_pool_workspace_ = nullptr;
        window_valid_ = false;
    }

    [[nodiscard]] bool has_item_pool_workspace() const noexcept {
        return item_pool_workspace_ != nullptr;
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
    }

    void set_selected(int index) noexcept {
        const int count = item_count_for_render();
        if (index < 0 || index >= count) return;
        selected_ = index;
        ensure_visible(index);
        if (select_fn_) select_fn_(select_ctx_, index);
    }

    int selected() const noexcept { return selected_; }

    void set_scroll_y(int y) noexcept {
        scroll_.set_scroll(y);
        notify_scroll();
        window_valid_ = false;
    }

    void add_scroll_y(int dy) noexcept {
        scroll_.add_scroll(dy);
        notify_scroll();
        window_valid_ = false;
    }

    void set_wheel_step(int step) noexcept { scroll_.wheel_step = step; }
    void set_show_scrollbar(bool on) noexcept { show_scrollbar_ = on; }

    int scroll_y() const noexcept { return scroll_.scroll_y; }
    int max_scroll() const noexcept { return scroll_.max_scroll; }
    int content_height() const noexcept { return scroll_.content_height; }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused));
        const Style& base = Theme::instance().get<ListView>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::ListView, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};

        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);

        const int pad = st.metrics.padding;
        const int content_x = r.x + pad;
        const int content_w = r.w - pad * 2;
        const int count = item_count_for_render();
        const int prefetch_rows = item_pool_workspace_ ? item_pool_workspace_->prefetch_rows_ : 0;
        if (!window_valid_ || window_prefetch_rows_ != prefetch_rows) update_visible_window();
        int start = window_start_;
        int visible = window_visible_;
        int y = window_offset_y_;
        const bool variable_height = (row_height_fn_ != nullptr);
        auto* item_pool = item_pool_workspace_;
        if (item_pool) item_pool->cache_.begin_frame();
        for (int i = start; i < count && y < r.y + r.h; ++i) {
            const int row_h = variable_height ? row_height_for_index(i) : row_height_for_render();
            Rect row{content_x, y, content_w, row_h};
            const bool is_selected = (i == selected_);
            if (is_selected) {
                draw_rect(cvs, row.x, row.y, row.w, row.h, accent, true);
            }
            int slot = -1;
            if (item_pool && visible > 0 && visible <= static_cast<int>(ItemPoolWorkspace::capacity)) {
                slot = i - start;
                if (slot >= 0 && slot < static_cast<int>(ItemPoolWorkspace::capacity)) {
                    item_pool->cache_.bind_slot(
                        slot,
                        i,
                        [&](int create_slot) {
                            if (item_pool->pool_create_fn_) {
                                item_pool->pool_create_fn_(item_pool->pool_ctx_, create_slot);
                            }
                        },
                        [&](int recycle_slot, int index) {
                            if (item_pool->pool_recycle_fn_) {
                                item_pool->pool_recycle_fn_(item_pool->pool_ctx_, recycle_slot, index);
                            }
                        },
                        [&](int bind_slot, int index) {
                            if (item_pool->pool_bind_fn_) {
                                item_pool->pool_bind_fn_(item_pool->pool_ctx_, bind_slot, index);
                            } else if (item_pool->cache_fn_) {
                                item_pool->cache_fn_(item_pool->cache_ctx_, bind_slot, index);
                            }
                        });
                } else {
                    slot = -1;
                }
            }
            if (draw_fn_) {
                draw_fn_(draw_ctx_, cvs, DrawInfo{row, i, is_selected, slot});
            }
            y += row_h;
        }

        if (item_pool) {
            item_pool->cache_.recycle_inactive([&](int slot, int index) {
                if (item_pool->pool_recycle_fn_) {
                    item_pool->pool_recycle_fn_(item_pool->pool_ctx_, slot, index);
                }
            });
        }

        cvs.restore_clip(clip_state);

        if (show_scrollbar_ && scroll_.max_scroll > 0) {
            const int margin = (st.metrics.scrollbar_margin >= 0) ? st.metrics.scrollbar_margin : 0;
            const int track_w = 6;
            const int track_x = r.x + r.w - track_w - margin;
            const int track_y = r.y + margin;
            const int track_h = r.h - margin * 2;
            const auto thumb = alg::scroll_thumb::vertical_from_maxscroll(
                track_x, track_y, track_w, track_h, r.h, scroll_.max_scroll, scroll_.scroll_y, st.metrics.scrollbar_thumb_min);
            if (thumb.visible && thumb.thumb_h > 0) {
                rgba thumb_col = st.colors.border_focus;
                thumb_col.a = 180;
                draw_rect(cvs, track_x, track_y, track_w, track_h, rgba{0,0,0,0}, false);
                draw_rect(cvs, thumb.thumb_x, thumb.thumb_y, thumb.thumb_w, thumb.thumb_h, thumb_col, true);
            }
        }

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
    }

    void update_visible_window() noexcept {
        Style st_scratch;
        const Style& st = resolve_style_for_state(st_scratch);
        const auto r = get_rect();
        update_scroll_bounds();
        const int pad = st.metrics.padding;
        const int count = item_count_for_render();
        const int prefetch_rows = item_pool_workspace_ ? item_pool_workspace_->prefetch_rows_ : 0;
        window_prefetch_rows_ = prefetch_rows;
        const bool variable_height = (row_height_fn_ != nullptr);
        int start = 0;
        int visible = 0;
        int y = r.y + pad;
        if (!variable_height) {
            const int row_h = row_height_for_render();
            StructuredViewportMapper mapper{};
            mapper.rect = Rect{r.x + pad, r.y + pad, r.w - pad * 2, r.h - pad * 2};
            mapper.row_height = row_h;
            mapper.scroll_y = scroll_.scroll_y;
            const StructuredVisibleRange range = mapper.visible_range(count);
            start = range.first;
            visible = (range.last >= range.first) ? (range.last - range.first + 1) : 0;
            const int row_offset = scroll_.scroll_y - start * row_h;
            y = r.y + pad - row_offset;
            if (prefetch_rows > 0 && count > 0) {
                int pref = prefetch_rows;
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
                if (acc + h > scroll_.scroll_y) {
                    start = i;
                    break;
                }
                acc += h;
                start = i + 1;
            }
            if (prefetch_rows > 0) {
                for (int p = 0; p < prefetch_rows && start > 0; ++p) {
                    --start;
                    acc -= row_height_for_index(start);
                }
            }
            y = r.y + pad - (scroll_.scroll_y - acc);
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

    bool on_event(const Event& e) {
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
            if (!dragging_) return false;
            dragging_ = false;
            return true;
        } else if (e.type == Event::Type::MouseWheel) {
            if (!r.contains(e.x, e.y)) return false;
            const int target_step = e.wheel_y * scroll_.wheel_step;
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
                if ((row_flags_for_index(index) & kStructuredListRowFlagDisabled) != 0) {
                    return true;
                }
                auto selection = make_selection_model();
                selection.set(index);
                return true;
            }
        } else if (e.type == Event::Type::KeyDown) {
            if (e.key_code == Event::Key::Up) {
                auto selection = make_selection_model();
                const int current = selection.current();
                if (current > 0) selection.set(current - 1);
                return true;
            }
            if (e.key_code == Event::Key::Down) {
                auto selection = make_selection_model();
                const int current = selection.current();
                if (current + 1 < item_count_for_render()) selection.set(current + 1);
                return true;
            }
        }
        return false;
    }

private:
    StyleState current_style_state() const noexcept {
        return make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed),
                                has_state(State::Focused));
    }

    const Style& resolve_style_for_state(Style& scratch) const noexcept {
        const Style& base = Theme::instance().get<ListView>();
        return resolve_style(WidgetKind::ListView, current_style_state(), base, scratch);
    }

    void update_scroll_bounds() noexcept {
        const auto r = get_rect();
        Style st_scratch;
        const Style& st = resolve_style_for_state(st_scratch);
        const int count = item_count_for_render();
        if (row_height_fn_) {
            int sum = 0;
            for (int i = 0; i < count; ++i) {
                sum += row_height_for_index(i);
            }
            scroll_.content_height = sum + st.metrics.padding * 2;
        } else {
            const int row_h = row_height_for_render();
            scroll_.content_height = count * row_h + st.metrics.padding * 2;
        }
        scroll_.set_content(scroll_.content_height, r.h);
        notify_scroll();
    }

    void ensure_visible(int index) noexcept {
        if (index < 0) return;
        Style st_scratch;
        const Style& st = resolve_style_for_state(st_scratch);
        const auto r = get_rect();
        const int pad = st.metrics.padding;
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
        const int view_top = scroll_.scroll_y;
        const int view_bottom = scroll_.scroll_y + (r.h - pad * 2);
        if (row_top < view_top) {
            set_scroll_y(row_top);
        } else if (row_bottom > view_bottom) {
            set_scroll_y(row_bottom - (r.h - pad * 2));
        }
    }

    int index_from_y(int y) const noexcept {
        Style st_scratch;
        const Style& st = resolve_style_for_state(st_scratch);
        const auto r = get_rect();
        if (!row_height_fn_) {
            const int row_h = row_height_for_render();
            StructuredViewportMapper mapper{};
            mapper.rect = Rect{r.x + st.metrics.padding, r.y + st.metrics.padding,
                               r.w - st.metrics.padding * 2, r.h - st.metrics.padding * 2};
            mapper.row_height = (row_h > 0) ? row_h : 1;
                mapper.scroll_y = scroll_.scroll_y;
                return mapper.index_at(y, item_count_for_render());
            }
        const int local = y - r.y + scroll_.scroll_y - st.metrics.padding;
        if (local < 0) return -1;
        int acc = 0;
        const int count = item_count_for_render();
        for (int i = 0; i < count; ++i) {
            acc += row_height_for_index(i);
            if (local < acc) return i;
        }
        return -1;
    }

    int item_count_for_render() const noexcept {
        const StructuredDataProvider provider = make_provider();
        if (provider.count) {
            return provider.size();
        }
        return (item_count_ > 0) ? item_count_ : 0;
    }

    int row_height_for_render() const noexcept {
        return (row_height_ > 4) ? row_height_ : 4;
    }

    int row_height_for_index(int index) const noexcept {
        if (!row_height_fn_) return row_height_for_render();
        int h = row_height_fn_(row_height_ctx_, index);
        return (h > 4) ? h : 4;
    }

    std::uint8_t row_flags_for_index(int index) const noexcept {
        if (!row_flags_fn_ || index < 0) return 0;
        return row_flags_fn_(row_flags_ctx_,
                             static_cast<std::uint16_t>(index));
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
        scroll_fn_(scroll_ctx_, scroll_.scroll_y, scroll_.max_scroll, r.h, scroll_.content_height);
    }

    StructuredDataProvider make_provider() const noexcept {
        return StructuredDataProvider{
            this,
            &ListView::provider_count,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        };
    }

    StructuredSelectionModel make_selection_model() noexcept {
        return StructuredSelectionModel{
            this,
            &ListView::selection_current,
            &ListView::selection_set,
            &ListView::selection_clear
        };
    }

    static std::uint16_t provider_count(const void* ctx) noexcept {
        const auto* self = static_cast<const ListView*>(ctx);
        if (!self) return 0;
        if (self->count_fn_) {
            const int count = self->count_fn_(self->data_ctx_);
            if (count <= 0) return 0;
            const int capped = (count > 0xFFFF) ? 0xFFFF : count;
            return static_cast<std::uint16_t>(capped);
        }
        if (self->item_count_ <= 0) return 0;
        const int capped = (self->item_count_ > 0xFFFF) ? 0xFFFF : self->item_count_;
        return static_cast<std::uint16_t>(capped);
    }

    static int selection_current(const void* ctx) noexcept {
        const auto* self = static_cast<const ListView*>(ctx);
        return self ? self->selected_ : -1;
    }

    static void selection_set(const void* ctx, int index) noexcept {
        auto* self = static_cast<ListView*>(const_cast<void*>(ctx));
        if (self) self->set_selected(index);
    }

    static void selection_clear(const void* ctx) noexcept {
        auto* self = static_cast<ListView*>(const_cast<void*>(ctx));
        if (!self) return;
        self->selected_ = -1;
    }

    void clear_cache() noexcept {
        if (item_pool_workspace_) item_pool_workspace_->recycle_cache();
    }

    void invalidate_cache_range(int start, int end) noexcept {
        if (!item_pool_workspace_) return;
        auto& workspace = *item_pool_workspace_;
        workspace.cache_.recycle_if([&](int slot, int index) {
            if (workspace.pool_recycle_fn_) workspace.pool_recycle_fn_(workspace.pool_ctx_, slot, index);
        }, [&](int index) {
            return index >= start && index < end;
        });
    }

    void invalidate_cache_from(int start) noexcept {
        if (!item_pool_workspace_) return;
        auto& workspace = *item_pool_workspace_;
        workspace.cache_.recycle_if([&](int slot, int index) {
            if (workspace.pool_recycle_fn_) workspace.pool_recycle_fn_(workspace.pool_ctx_, slot, index);
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
    ItemPoolWorkspace* item_pool_workspace_{nullptr};
    RowHeightFn row_height_fn_{nullptr};
    void* row_height_ctx_{nullptr};
    RowFlagsFn row_flags_fn_{nullptr};
    void* row_flags_ctx_{nullptr};
    ScrollFn scroll_fn_{nullptr};
    void* scroll_ctx_{nullptr};

    int item_count_{0};
    int row_height_{24};
    int selected_{-1};
    StructuredScrollModel scroll_{};
    bool dragging_{false};
    int last_y_{0};
    bool show_scrollbar_{true};
    bool swipe_active_{false};
    int window_start_{0};
    int window_visible_{0};
    int window_offset_y_{0};
    int window_prefetch_rows_{-1};
    bool window_valid_{false};
};

inline ListView::ItemPoolWorkspace::~ItemPoolWorkspace() noexcept {
    if (owner_) owner_->detach_item_pool_workspace();
}





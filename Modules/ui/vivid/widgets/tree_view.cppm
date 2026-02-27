module;
#include <algorithm>
export module charm.widgets.tree_view;

import charm.core.object;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;
import charm.core.style_sheet;
import charm.core.virtual_list;
import charm.widgets.text;
import alg_scroll_bounds;
import alg_list_scroll;

using namespace ui::render;

export
class TreeView : public WidgetBase<TreeView> {
public:
    struct NodeInfo {
        int depth{0};
        bool expanded{false};
        bool has_children{false};
        const char* label{nullptr};
    };

    struct DrawInfo {
        Rect rect{};
        int index{0};
        bool selected{false};
        NodeInfo node{};
        int slot{-1};
    };

    using CountFn = int(*)(void* ctx) noexcept;
    using NodeFn = NodeInfo(*)(void* ctx, int index) noexcept;
    using DrawNodeFn = void(*)(void* ctx, CanvasBase& cvs, const DrawInfo& info) noexcept;
    using ToggleFn = void(*)(void* ctx, int index) noexcept;
    using SelectFn = void(*)(void* ctx, int index) noexcept;
    using PoolCreateFn = void(*)(void* ctx, int slot) noexcept;
    using PoolBindFn = void(*)(void* ctx, int slot, int index, const NodeInfo& info) noexcept;
    using PoolRecycleFn = void(*)(void* ctx, int slot, int index) noexcept;
    using RowHeightFn = int(*)(void* ctx, int index, const NodeInfo& info) noexcept;

    TreeView() {
        set_focusable(true);
        set_size(260, 180);
    }

    void set_data_source(CountFn count_fn, NodeFn node_fn, DrawNodeFn draw_fn, void* ctx = nullptr) noexcept {
        count_fn_ = count_fn;
        node_fn_ = node_fn;
        draw_fn_ = draw_fn;
        data_ctx_ = ctx;
        clear_cache();
        update_scroll_bounds();
    }

    void set_row_height(int h) noexcept {
        row_height_ = (h > 6) ? h : 6;
        row_height_fn_ = nullptr;
        row_height_ctx_ = nullptr;
        clear_cache();
        update_scroll_bounds();
    }

    void set_row_height_fn(RowHeightFn fn, void* ctx = nullptr) noexcept {
        row_height_fn_ = fn;
        row_height_ctx_ = ctx;
        clear_cache();
        update_scroll_bounds();
    }

    void set_on_toggle(ToggleFn fn) noexcept { toggle_fn_ = fn; }
    void set_on_select(SelectFn fn, void* ctx = nullptr) noexcept {
        select_fn_ = fn;
        select_ctx_ = ctx;
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

    void set_prefetch_rows(int rows) noexcept {
        prefetch_rows_ = (rows > 0) ? rows : 0;
        clear_cache();
    }

    void set_selected(int index) noexcept {
        selected_ = index;
        if (select_fn_) select_fn_(select_ctx_, index);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<TreeView>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::TreeView, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int count = item_count();
        if (count <= 0) return;

        update_scroll_bounds();
        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);

        const bool variable_height = (row_height_fn_ != nullptr);
        int start = 0;
        int visible = 0;
        int y = r.y;
        if (!variable_height) {
            const auto window = compute_virtual_window(scroll_y_, row_height_, r.h, r.y, prefetch_rows_);
            start = window.start;
            visible = window.visible;
            y = window.offset_y;
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
            y = r.y - (scroll_y_ - acc);
            int temp_y = y;
            for (int i = start; i < count && temp_y < r.y + r.h; ++i) {
                temp_y += row_height_for_index(i);
                ++visible;
            }
        }
        auto on_create = [&](int slot) {
            if (pool_create_fn_) pool_create_fn_(pool_ctx_, slot);
        };
        auto on_recycle = [&](int slot, int index) {
            if (pool_recycle_fn_) pool_recycle_fn_(pool_ctx_, slot, index);
        };
        cache_.begin_frame();
        for (int i = start; i < count && y < r.y + r.h; ++i) {
            NodeInfo info = node_fn_ ? node_fn_(data_ctx_, i) : NodeInfo{};
            const int row_h = variable_height ? row_height_for_index(i, info) : row_height_;
            Rect row{r.x, y, r.w, row_h};
            const bool selected = (i == selected_);
            if (selected) {
                draw_rect(cvs, row.x, row.y, row.w, row.h, accent, true);
            }
            int slot = -1;
            if (visible > 0 && visible <= kMaxCache) {
                slot = i - start;
                if (slot >= 0 && slot < kMaxCache) {
                    cache_.bind_slot(slot, i, on_create, on_recycle,
                                     [&](int bind_slot, int index) {
                                         if (pool_bind_fn_) {
                                             pool_bind_fn_(pool_ctx_, bind_slot, index, info);
                                         }
                                     });
                } else {
                    slot = -1;
                }
            }
            draw_node_glyphs(cvs, row, info, border);
            if (draw_fn_) {
                draw_fn_(data_ctx_, cvs, DrawInfo{row, i, selected, info, slot});
            } else if (info.label) {
                Rect label_box{row.x + st.metrics.padding + info.depth * indent_w_, row.y, row.w, row.h};
                draw_text_box(cvs, label_box, info.label, font, resolve_font(st),
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            }
            y += row_h;
        }

        cache_.recycle_inactive(on_recycle);

        cvs.restore_clip(clip_state);

        draw_focus_ring(cvs, r, st, has_state(State::Focused));
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        const auto r = get_rect();
        if (e.type == Event::Type::MouseWheel) {
            if (!r.contains(e.x, e.y)) return false;
            add_scroll_y(-e.wheel_y * wheel_step_);
            return true;
        } else if (e.type == Event::Type::Click) {
            if (!r.contains(e.x, e.y)) return false;
            const int index = index_from_y(e.y);
            if (index >= 0 && index < item_count()) {
                const NodeInfo info = node_fn_ ? node_fn_(data_ctx_, index) : NodeInfo{};
                const int toggle_x = r.x + info.depth * indent_w_;
                if (info.has_children && e.x >= toggle_x && e.x < toggle_x + indent_w_) {
                    if (toggle_fn_) toggle_fn_(data_ctx_, index);
                }
                set_selected(index);
                return true;
            }
        }
        return false;
    }

private:
    int item_count() const noexcept {
        if (count_fn_) {
            const int v = count_fn_(data_ctx_);
            return (v > 0) ? v : 0;
        }
        return 0;
    }

    int index_from_y(int y) const noexcept {
        const auto r = get_rect();
        if (row_height_ <= 0) return -1;
        if (!row_height_fn_) {
            return alg::list_scroll::index_from_y(y, r.y, scroll_y_, 0, row_height_, item_count());
        }
        const int local = y - r.y + scroll_y_;
        if (local < 0) return -1;
        int acc = 0;
        const int count = item_count();
        for (int i = 0; i < count; ++i) {
            acc += row_height_for_index(i);
            if (local < acc) return i;
        }
        return -1;
    }

    void update_scroll_bounds() noexcept {
        const auto r = get_rect();
        if (!row_height_fn_) {
            const auto bounds = alg::list_scroll::compute_bounds(item_count(), row_height_, 0, r.h);
            max_scroll_y_ = bounds.max_scroll;
        } else {
            int total_h = 0;
            const int count = item_count();
            for (int i = 0; i < count; ++i) {
                total_h += row_height_for_index(i);
            }
            max_scroll_y_ = alg::scroll_bounds::compute_max(total_h, r.h);
        }
        scroll_y_ = alg::scroll_bounds::clamp(scroll_y_, max_scroll_y_);
    }

    void add_scroll_y(int dy) noexcept {
        scroll_y_ = alg::scroll_bounds::clamp(scroll_y_ + dy, max_scroll_y_);
    }

    void draw_node_glyphs(CanvasBase& cvs, const Rect& row, const NodeInfo& info, const rgba& color) const noexcept {
        if (!info.has_children) return;
        const int cx = row.x + info.depth * indent_w_ + indent_w_ / 2;
        const int cy = row.y + row.h / 2;
        const int s = 4;
        draw_line(cvs, cx - s, cy, cx + s, cy, color);
        if (!info.expanded) {
            draw_line(cvs, cx, cy - s, cx, cy + s, color);
        }
    }

    void clear_cache() noexcept {
        cache_.clear([&](int slot, int index) {
            if (pool_recycle_fn_) pool_recycle_fn_(pool_ctx_, slot, index);
        });
    }

    static constexpr int indent_w_ = 12;
    CountFn count_fn_{nullptr};
    NodeFn node_fn_{nullptr};
    DrawNodeFn draw_fn_{nullptr};
    ToggleFn toggle_fn_{nullptr};
    SelectFn select_fn_{nullptr};
    PoolCreateFn pool_create_fn_{nullptr};
    PoolBindFn pool_bind_fn_{nullptr};
    PoolRecycleFn pool_recycle_fn_{nullptr};
    void* pool_ctx_{nullptr};
    RowHeightFn row_height_fn_{nullptr};
    void* row_height_ctx_{nullptr};
    void* data_ctx_{nullptr};
    void* select_ctx_{nullptr};
    int row_height_{20};
    int scroll_y_{0};
    int max_scroll_y_{0};
    int wheel_step_{24};
    int selected_{-1};
    int prefetch_rows_{1};

    static constexpr int kMaxCache = 32;
    VirtualListCache<kMaxCache> cache_{};

    int row_height_for_index(int index) const noexcept {
        if (!row_height_fn_) return row_height_;
        const NodeInfo info = node_fn_ ? node_fn_(data_ctx_, index) : NodeInfo{};
        return row_height_for_index(index, info);
    }

    int row_height_for_index(int index, const NodeInfo& info) const noexcept {
        if (!row_height_fn_) return row_height_;
        int h = row_height_fn_(row_height_ctx_ ? row_height_ctx_ : data_ctx_, index, info);
        return (h > 6) ? h : 6;
    }
};





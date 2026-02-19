module;
#include <algorithm>
export module charm.widgets.tree_view;

import charm.core.object;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;
import charm.widgets.text;

using namespace ui::render;

export
class TreeView : public ObjectBase {
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
    };

    using CountFn = int(*)(void* ctx) noexcept;
    using NodeFn = NodeInfo(*)(void* ctx, int index) noexcept;
    using DrawNodeFn = void(*)(void* ctx, DefaultCanvas& cvs, const DrawInfo& info) noexcept;
    using ToggleFn = void(*)(void* ctx, int index) noexcept;
    using SelectFn = void(*)(void* ctx, int index) noexcept;

    TreeView() {
        set_focusable(true);
        set_size(260, 180);
    }

    void set_data_source(CountFn count_fn, NodeFn node_fn, DrawNodeFn draw_fn, void* ctx = nullptr) noexcept {
        count_fn_ = count_fn;
        node_fn_ = node_fn;
        draw_fn_ = draw_fn;
        data_ctx_ = ctx;
        update_scroll_bounds();
    }

    void set_row_height(int h) noexcept {
        row_height_ = (h > 6) ? h : 6;
        update_scroll_bounds();
    }

    void set_on_toggle(ToggleFn fn) noexcept { toggle_fn_ = fn; }
    void set_on_select(SelectFn fn, void* ctx = nullptr) noexcept {
        select_fn_ = fn;
        select_ctx_ = ctx;
    }

    void set_selected(int index) noexcept {
        selected_ = index;
        if (select_fn_) select_fn_(select_ctx_, index);
    }

    void draw(DefaultCanvas& cvs) override {
        const Style& st = Theme::instance().get<TreeView>();
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st,
                       {is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused)},
                       bg, border, font);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int count = item_count();
        if (count <= 0) return;

        update_scroll_bounds();
        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);

        const int start = (row_height_ > 0) ? (scroll_y_ / row_height_) : 0;
        int y = r.y - (scroll_y_ % row_height_);
        for (int i = start; i < count && y < r.y + r.h; ++i) {
            NodeInfo info = node_fn_ ? node_fn_(data_ctx_, i) : NodeInfo{};
            Rect row{r.x, y, r.w, row_height_};
            const bool selected = (i == selected_);
            if (selected) {
                draw_rect(cvs, row.x, row.y, row.w, row.h, st.bg_pressed, true);
            }
            draw_node_glyphs(cvs, row, info, border);
            if (draw_fn_) {
                draw_fn_(data_ctx_, cvs, DrawInfo{row, i, selected, info});
            } else if (info.label) {
                Rect label_box{row.x + st.padding + info.depth * indent_w_, row.y, row.w, row.h};
                draw_text_box(cvs, label_box, info.label, font, resolve_font(st),
                              TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
            }
            y += row_height_;
        }

        cvs.restore_clip(clip_state);

        if (has_state(State::Focused)) {
            draw_rect(cvs, r.x, r.y, r.w, r.h, st.border_focus, false);
        }
    }

    bool on_event(const Event& e) override {
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
        const int local = y - r.y + scroll_y_;
        if (local < 0 || row_height_ <= 0) return -1;
        return local / row_height_;
    }

    void update_scroll_bounds() noexcept {
        const auto r = get_rect();
        const int total_h = item_count() * row_height_;
        max_scroll_y_ = std::max(0, total_h - r.h);
        if (scroll_y_ > max_scroll_y_) scroll_y_ = max_scroll_y_;
        if (scroll_y_ < 0) scroll_y_ = 0;
    }

    void add_scroll_y(int dy) noexcept {
        scroll_y_ += dy;
        if (scroll_y_ < 0) scroll_y_ = 0;
        if (scroll_y_ > max_scroll_y_) scroll_y_ = max_scroll_y_;
    }

    void draw_node_glyphs(DefaultCanvas& cvs, const Rect& row, const NodeInfo& info, const rgba& color) const noexcept {
        if (!info.has_children) return;
        const int cx = row.x + info.depth * indent_w_ + indent_w_ / 2;
        const int cy = row.y + row.h / 2;
        const int s = 4;
        draw_line(cvs, cx - s, cy, cx + s, cy, color);
        if (!info.expanded) {
            draw_line(cvs, cx, cy - s, cx, cy + s, color);
        }
    }

    static constexpr int indent_w_ = 12;
    CountFn count_fn_{nullptr};
    NodeFn node_fn_{nullptr};
    DrawNodeFn draw_fn_{nullptr};
    ToggleFn toggle_fn_{nullptr};
    SelectFn select_fn_{nullptr};
    void* data_ctx_{nullptr};
    void* select_ctx_{nullptr};
    int row_height_{20};
    int scroll_y_{0};
    int max_scroll_y_{0};
    int wheel_step_{24};
    int selected_{-1};
};

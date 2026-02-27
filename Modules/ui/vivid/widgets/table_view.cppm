module;
#include <algorithm>
export module charm.widgets.table_view;

import charm.core.object;
import charm.core.event;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.text;
import alg_scroll_bounds;
import alg_list_scroll;

using namespace ui::render;

export
class TableView : public WidgetBase<TableView> {
public:
    struct CellInfo {
        Rect rect{};
        int row{0};
        int col{0};
        bool selected{false};
    };

    using DrawCellFn = void(*)(void* ctx, CanvasBase& cvs, const CellInfo& info) noexcept;
    using CountFn = int(*)(void* ctx) noexcept;
    using ColumnWidthFn = int(*)(void* ctx, int col) noexcept;
    using SelectFn = void(*)(void* ctx, int row, int col) noexcept;

    TableView() {
        set_focusable(true);
        set_size(320, 180);
    }

    void set_data_source(CountFn rows, CountFn cols, DrawCellFn draw, void* ctx = nullptr) noexcept {
        row_count_fn_ = rows;
        col_count_fn_ = cols;
        draw_fn_ = draw;
        data_ctx_ = ctx;
        update_scroll_bounds();
    }

    void set_row_height(int h) noexcept {
        row_height_ = (h > 6) ? h : 6;
        update_scroll_bounds();
    }

    void set_column_width(int col, int w) noexcept {
        if (col < 0 || col >= kMaxCols) return;
        col_widths_[col] = (w > 8) ? w : 8;
    }

    void set_column_width_fn(ColumnWidthFn fn) noexcept { col_width_fn_ = fn; }

    void set_on_select(SelectFn fn, void* ctx = nullptr) noexcept {
        select_fn_ = fn;
        select_ctx_ = ctx;
    }

    void set_selected(int row, int col) noexcept {
        selected_row_ = row;
        selected_col_ = col;
        if (select_fn_) select_fn_(select_ctx_, row, col);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<TableView>();
        Style st_scratch{};
        const Style& st = resolve_style(WidgetKind::TableView, state, base, st_scratch);
        const auto r = get_rect();

        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        draw_rect(cvs, r.x, r.y, r.w, r.h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, r.h, border, false);

        const int rows = row_count();
        const int cols = col_count();
        if (rows <= 0 || cols <= 0) return;

        update_scroll_bounds();
        auto clip_state = cvs.save_clip();
        cvs.set_clip(r);

        int y = r.y - scroll_y_;
        for (int row = 0; row < rows && y < r.y + r.h; ++row) {
            int x = r.x - scroll_x_;
            for (int col = 0; col < cols && x < r.x + r.w; ++col) {
                const int w = column_width(col);
                Rect cell{x, y, w, row_height_};
                if (cell.x + cell.w > r.x && cell.y + cell.h > r.y &&
                    cell.x < r.x + r.w && cell.y < r.y + r.h) {
                    const bool selected = (row == selected_row_ && col == selected_col_);
                    if (selected) {
                        draw_rect(cvs, cell.x, cell.y, cell.w, cell.h, accent, true);
                    }
                    if (draw_fn_) {
                        draw_fn_(data_ctx_, cvs, CellInfo{cell, row, col, selected});
                    }
                    draw_rect(cvs, cell.x, cell.y, cell.w, cell.h, border, false);
                }
                x += w;
            }
            y += row_height_;
        }

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
            const int row = (e.y - r.y + scroll_y_) / row_height_;
            int col = 0;
            int acc = r.x - scroll_x_;
            for (int i = 0; i < col_count(); ++i) {
                const int w = column_width(i);
                if (e.x >= acc && e.x < acc + w) { col = i; break; }
                acc += w;
            }
            set_selected(row, col);
            return true;
        }
        return false;
    }

private:
    static constexpr int kMaxCols = 16;

    int row_count() const noexcept {
        if (row_count_fn_) {
            const int v = row_count_fn_(data_ctx_);
            return (v > 0) ? v : 0;
        }
        return row_count_;
    }

    int col_count() const noexcept {
        if (col_count_fn_) {
            const int v = col_count_fn_(data_ctx_);
            return (v > 0) ? v : 0;
        }
        return col_count_;
    }

    int column_width(int col) const noexcept {
        if (col < 0 || col >= kMaxCols) return 24;
        if (col_width_fn_) {
            const int v = col_width_fn_(data_ctx_, col);
            return (v > 8) ? v : 8;
        }
        const int w = col_widths_[col];
        return (w > 8) ? w : 8;
    }

    void update_scroll_bounds() noexcept {
        const auto r = get_rect();
        const int rows = row_count();
        const int cols = col_count();
        int total_w = 0;
        for (int i = 0; i < cols; ++i) total_w += column_width(i);
        max_scroll_x_ = alg::scroll_bounds::compute_max(total_w, r.w);
        const auto bounds = alg::list_scroll::compute_bounds(rows, row_height_, 0, r.h);
        max_scroll_y_ = bounds.max_scroll;
        scroll_x_ = alg::scroll_bounds::clamp(scroll_x_, max_scroll_x_);
        scroll_y_ = alg::scroll_bounds::clamp(scroll_y_, max_scroll_y_);
    }

    void add_scroll_y(int dy) noexcept {
        scroll_y_ = alg::scroll_bounds::clamp(scroll_y_ + dy, max_scroll_y_);
    }

    CountFn row_count_fn_{nullptr};
    CountFn col_count_fn_{nullptr};
    DrawCellFn draw_fn_{nullptr};
    ColumnWidthFn col_width_fn_{nullptr};
    void* data_ctx_{nullptr};
    SelectFn select_fn_{nullptr};
    void* select_ctx_{nullptr};

    int row_count_{0};
    int col_count_{0};
    int row_height_{20};
    int col_widths_[kMaxCols]{};
    int scroll_x_{0};
    int scroll_y_{0};
    int max_scroll_x_{0};
    int max_scroll_y_{0};
    int wheel_step_{24};
    int selected_row_{-1};
    int selected_col_{-1};
};





module;
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
export module charm.widgets.table_view;

import charm.core.object;
import charm.core.event;
import charm.core.structured_view;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.style;
import charm.core.style_sheet;
import charm.gfx.text_box;
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
    static constexpr std::size_t max_columns = 16;

    TableView() {
        set_focusable(true);
        set_size(320, 180);
    }

    ~TableView() noexcept {
        detach_column_width_storage();
    }

    TableView(const TableView&) = delete;
    TableView& operator=(const TableView&) = delete;
    TableView(TableView&&) = delete;
    TableView& operator=(TableView&&) = delete;

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

    [[nodiscard]] bool attach_column_width_storage(std::span<int> widths) noexcept {
        if (widths.size() > max_columns) return false;
        detach_column_width_storage();
        for (auto& width : widths) width = 0;
        col_widths_ = widths;
        return true;
    }

    void detach_column_width_storage() noexcept {
        for (auto& width : col_widths_) width = 0;
        col_widths_ = {};
    }

    [[nodiscard]] std::size_t column_width_storage_capacity() const noexcept {
        return col_widths_.size();
    }

    [[nodiscard]] bool set_column_width(int col, int w) noexcept {
        if (col < 0 || static_cast<std::size_t>(col) >= col_widths_.size()) return false;
        col_widths_[static_cast<std::size_t>(col)] = (w > 8) ? w : 8;
        update_scroll_bounds();
        return true;
    }

    void set_column_width_fn(ColumnWidthFn fn, void* ctx = nullptr) noexcept {
        col_width_fn_ = fn;
        col_width_ctx_ = ctx;
        update_scroll_bounds();
    }

    void set_on_select(SelectFn fn, void* ctx = nullptr) noexcept {
        select_fn_ = fn;
        select_ctx_ = ctx;
    }

    void set_selected(int row, int col) noexcept {
        const int rows = row_count();
        if (row < 0 || row >= rows) return;
        if (col < 0 || col >= col_count()) col = 0;
        selected_row_ = row;
        selected_col_ = col;
        if (select_fn_) select_fn_(select_ctx_, row, col);
    }

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused));
        const Style& base = Theme::instance().get<TableView>();
        Style st_scratch;
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

        StructuredViewportMapper mapper{};
        mapper.rect = r;
        mapper.row_height = row_height_;
        mapper.scroll_y = scroll_.scroll_y;
        const StructuredVisibleRange range = mapper.visible_range(rows);
        int y = r.y + range.first * row_height_ - scroll_.scroll_y;
        for (int row = range.first; row <= range.last; ++row) {
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
            add_scroll_y(-e.wheel_y * scroll_.wheel_step);
            return true;
        } else if (e.type == Event::Type::Click) {
            if (!r.contains(e.x, e.y)) return false;
            const int rows = row_count();
            const int cols = col_count();
            if (rows <= 0 || cols <= 0) return false;
            StructuredViewportMapper mapper{};
            mapper.rect = r;
            mapper.row_height = row_height_;
            mapper.scroll_y = scroll_.scroll_y;
            const int row = mapper.index_at(e.y, rows);
            if (row < 0) return false;

            int col = -1;
            int acc = r.x - scroll_x_;
            for (int i = 0; i < cols; ++i) {
                const int w = column_width(i);
                if (e.x >= acc && e.x < acc + w) {
                    col = i;
                    break;
                }
                acc += w;
            }
            if (col < 0) return false;

            set_selected(row, col);
            return true;
        }
        return false;
    }

private:
    int row_count() const noexcept {
        const StructuredDataProvider provider = make_provider();
        if (provider.count) {
            return provider.size();
        }
        return (row_count_ > 0) ? row_count_ : 0;
    }

    int col_count() const noexcept {
        int count = col_count_;
        if (col_count_fn_) {
            count = col_count_fn_(data_ctx_);
        }
        if (count <= 0) return 0;
        return std::min(count, static_cast<int>(max_columns));
    }

    int column_width(int col) const noexcept {
        if (col < 0 || col >= static_cast<int>(max_columns)) return 24;
        if (col_width_fn_) {
            const int v = col_width_fn_(col_width_ctx_, col);
            return (v > 8) ? v : 8;
        }
        if (static_cast<std::size_t>(col) >= col_widths_.size()) return 24;
        const int w = col_widths_[static_cast<std::size_t>(col)];
        return (w > 8) ? w : 8;
    }

    void update_scroll_bounds() noexcept {
        const auto r = get_rect();
        const int rows = row_count();
        const int cols = col_count();
        int total_w = 0;
        for (int i = 0; i < cols; ++i) total_w += column_width(i);
        max_scroll_x_ = alg::scroll_bounds::compute_max(total_w, r.w);
        scroll_x_ = alg::scroll_bounds::clamp(scroll_x_, max_scroll_x_);
        const int content_h = rows * row_height_;
        scroll_.set_content(content_h, r.h);
    }

    void add_scroll_y(int dy) noexcept {
        scroll_.add_scroll(dy);
    }

    StructuredDataProvider make_provider() const noexcept {
        return StructuredDataProvider{
            this,
            &TableView::provider_count,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        };
    }

    StructuredSelectionModel make_selection_model() noexcept {
        return StructuredSelectionModel{
            this,
            &TableView::selection_current,
            &TableView::selection_set,
            &TableView::selection_clear
        };
    }

    static std::uint16_t provider_count(const void* ctx) noexcept {
        const auto* self = static_cast<const TableView*>(ctx);
        if (!self) return 0;
        if (self->row_count_fn_) {
            const int v = self->row_count_fn_(self->data_ctx_);
            if (v <= 0) return 0;
            const int capped = (v > 0xFFFF) ? 0xFFFF : v;
            return static_cast<std::uint16_t>(capped);
        }
        if (self->row_count_ <= 0) return 0;
        const int capped = (self->row_count_ > 0xFFFF) ? 0xFFFF : self->row_count_;
        return static_cast<std::uint16_t>(capped);
    }

    static int selection_current(const void* ctx) noexcept {
        const auto* self = static_cast<const TableView*>(ctx);
        return self ? self->selected_row_ : -1;
    }

    static void selection_set(const void* ctx, int row) noexcept {
        auto* self = static_cast<TableView*>(const_cast<void*>(ctx));
        if (!self) return;
        const int col = (self->selected_col_ >= 0) ? self->selected_col_ : 0;
        self->set_selected(row, col);
    }

    static void selection_clear(const void* ctx) noexcept {
        auto* self = static_cast<TableView*>(const_cast<void*>(ctx));
        if (!self) return;
        self->selected_row_ = -1;
        self->selected_col_ = -1;
    }

    CountFn row_count_fn_{nullptr};
    CountFn col_count_fn_{nullptr};
    DrawCellFn draw_fn_{nullptr};
    ColumnWidthFn col_width_fn_{nullptr};
    void* data_ctx_{nullptr};
    void* col_width_ctx_{nullptr};
    SelectFn select_fn_{nullptr};
    void* select_ctx_{nullptr};

    int row_count_{0};
    int col_count_{0};
    int row_height_{20};
    std::span<int> col_widths_{};
    int scroll_x_{0};
    StructuredScrollModel scroll_{};
    int max_scroll_x_{0};
    int selected_row_{-1};
    int selected_col_{-1};
};

static_assert(sizeof(TableView)
              <= sizeof(ObjectBase) + sizeof(void*) * 8 + sizeof(std::span<int>)
                  + sizeof(int) * 8 + sizeof(StructuredScrollModel)
                  + alignof(TableView) * 3,
              "TableView must not regain a fixed column width table");
static_assert(!std::is_copy_constructible_v<TableView>);
static_assert(!std::is_move_constructible_v<TableView>);





module;
#include <cstddef>
#include <array>
export module charm.core.layout;

import charm.core.object;
import charm.core.factory;

export
class Layout {
public:
    virtual ~Layout() = default;
    // Use container size and child preferred size to compute layout.
    // Set each child x/y/width/height after layout.
    virtual void apply(UiFactory& factory, ObjectBase& container) = 0;
};

export
class AbsoluteLayout : public Layout {
public:
    void apply(UiFactory&, ObjectBase&) override {
        // No change; keep each child x,y as-is.
    }
};

export
class BoxLayout : public Layout {
public:
    enum class Direction { Horizontal, Vertical };

    BoxLayout(Direction dir, int spacing=0, int padding=0)
      : dir(dir), spacing(spacing), padding(padding) {}

    void apply(UiFactory& factory, ObjectBase& container) override {
        auto rect = container.layout_rect();
        int offset = (dir == Direction::Horizontal ? rect.x : rect.y) + padding;

        for (std::size_t i = 0; i < container.child_count(); ++i) {
            auto h = container.child_at(i);
            auto* ch = factory.get(h);
            if (!ch) continue;
            auto r = ch->get_rect();
            if (dir == Direction::Horizontal) {
                ch->set_pos(offset, rect.y + padding);
                offset += r.w + spacing;
            } else {
                ch->set_pos(rect.x + padding, offset);
                offset += r.h + spacing;
            }
        }
    }

private:
    Direction dir;
    int spacing;
    int padding;
};

export
using LayoutHandler = void(*)(UiFactory&, ObjectBase&, const ObjectBase::LayoutSpec&);

inline constexpr int kMaxLayoutEngines = 8;
inline LayoutHandler g_layout_engines[kMaxLayoutEngines]{};

export
inline bool register_layout_engine(int id, LayoutHandler handler) noexcept {
    if (id <= 0 || id >= kMaxLayoutEngines) return false;
    g_layout_engines[id] = handler;
    return true;
}

export
inline void clear_layout_engine(int id) noexcept {
    if (id <= 0 || id >= kMaxLayoutEngines) return;
    g_layout_engines[id] = nullptr;
}

inline bool apply_custom_layout(UiFactory& factory, ObjectBase& container,
                                const ObjectBase::LayoutSpec& spec) {
    const int id = spec.custom_id;
    if (id <= 0 || id >= kMaxLayoutEngines) return false;
    auto handler = g_layout_engines[id];
    if (!handler) return false;
    handler(factory, container, spec);
    return true;
}

export
enum class FlexFlow {
    Row,
    Column
};

export
enum class FlexAlign {
    Start,
    Center,
    End,
    SpaceAround,
    SpaceBetween,
    SpaceEvenly
};

export
enum class FlexCrossAlign {
    Start,
    Center,
    End,
    Stretch
};

export
enum class AlignH {
    Start,
    Center,
    End,
    Stretch
};

export
enum class AlignV {
    Start,
    Center,
    End,
    Stretch
};

export
struct FlexLayoutConfig {
    FlexFlow flow{FlexFlow::Row};
    FlexAlign main_align{FlexAlign::Start};
    FlexCrossAlign cross_align{FlexCrossAlign::Start};
    int gap{0};
    int padding{0};
};

export
inline void apply_flex_layout(UiFactory& factory, ObjectBase& container, const FlexLayoutConfig& cfg) {
    const auto rect = container.layout_rect();
    const int inner_x = rect.x + cfg.padding;
    const int inner_y = rect.y + cfg.padding;
    const int inner_w = rect.w - cfg.padding * 2;
    const int inner_h = rect.h - cfg.padding * 2;

    int total_main = 0;
    int count = 0;
    int total_grow = 0;
    for (std::size_t i = 0; i < container.child_count(); ++i) {
        auto h = container.child_at(i);
        auto* ch = factory.get(h);
        if (!ch || !ch->is_visible()) continue;
        auto r = ch->get_rect();
        total_main += (cfg.flow == FlexFlow::Row) ? r.w : r.h;
        total_grow += ch->flex_grow();
        ++count;
    }
    if (count == 0) return;
    total_main += cfg.gap * (count - 1);

    int start_main = (cfg.flow == FlexFlow::Row) ? inner_x : inner_y;
    int extra = ((cfg.flow == FlexFlow::Row) ? inner_w : inner_h) - total_main;
    int gap = cfg.gap;
    if (extra < 0) extra = 0;

    switch (cfg.main_align) {
        case FlexAlign::Start: break;
        case FlexAlign::Center: start_main += extra / 2; break;
        case FlexAlign::End: start_main += extra; break;
        case FlexAlign::SpaceAround: gap = cfg.gap + (extra / count); start_main += gap / 2; break;
        case FlexAlign::SpaceBetween: gap = (count > 1) ? cfg.gap + (extra / (count - 1)) : 0; break;
        case FlexAlign::SpaceEvenly: gap = cfg.gap + (extra / (count + 1)); start_main += gap; break;
    }

    int cursor = start_main;
    for (std::size_t i = 0; i < container.child_count(); ++i) {
        auto h = container.child_at(i);
        auto* ch = factory.get(h);
        if (!ch || !ch->is_visible()) continue;
        auto r = ch->get_rect();

        int cross_pos = (cfg.flow == FlexFlow::Row) ? inner_y : inner_x;
        int cross_space = (cfg.flow == FlexFlow::Row) ? inner_h : inner_w;
        int cross_size = (cfg.flow == FlexFlow::Row) ? r.h : r.w;
        if (cfg.cross_align == FlexCrossAlign::Stretch) {
            cross_size = cross_space;
        }
        if (extra > 0 && total_grow > 0) {
            const int grow = ch->flex_grow();
            if (grow > 0) {
                const int add = (extra * grow) / total_grow;
                if (cfg.flow == FlexFlow::Row) r.w += add;
                else r.h += add;
            }
        }

        switch (cfg.cross_align) {
            case FlexCrossAlign::Start: break;
            case FlexCrossAlign::Center: cross_pos += (cross_space - cross_size) / 2; break;
            case FlexCrossAlign::End: cross_pos += (cross_space - cross_size); break;
            case FlexCrossAlign::Stretch: break;
        }

        if (cfg.flow == FlexFlow::Row) {
            ch->set_pos(cursor, cross_pos);
            ch->set_size(r.w, cross_size);
            cursor += r.w + gap;
        } else {
            ch->set_pos(cross_pos, cursor);
            ch->set_size(cross_size, r.h);
            cursor += r.h + gap;
        }
    }
}

export
inline void apply_flex_layout(UiFactory& factory, ObjectBase& container) {
    if (!container.has_flex_layout()) return;
    FlexLayoutConfig cfg;
    cfg.flow = static_cast<FlexFlow>(container.flex_flow());
    cfg.main_align = static_cast<FlexAlign>(container.flex_main_align());
    cfg.cross_align = static_cast<FlexCrossAlign>(container.flex_cross_align());
    cfg.gap = container.flex_gap();
    cfg.padding = container.flex_padding();
    apply_flex_layout(factory, container, cfg);
}

export
inline void apply_flow_layout(UiFactory& factory, ObjectBase& container,
                              int gap, int line_gap, int padding) {
    const auto rect = container.layout_rect();
    const int inner_x = rect.x + padding;
    const int inner_y = rect.y + padding;
    const int inner_w = rect.w - padding * 2;
    const int max_x = inner_x + inner_w;
    const int align_h = container.align_h();

    int cursor_x = inner_x;
    int cursor_y = inner_y;
    int line_h = 0;
    int line_w = 0;
    constexpr std::size_t kMaxLine = 64;
    std::array<WidgetHandle, kMaxLine> line{};
    std::array<int, kMaxLine> line_x{};
    std::size_t line_count = 0;

    auto flush_line = [&]() {
        if (line_count == 0) return;
        int offset = 0;
        if (align_h == static_cast<int>(AlignH::Center)) {
            offset = (inner_w - line_w) / 2;
        } else if (align_h == static_cast<int>(AlignH::End)) {
            offset = inner_w - line_w;
        }
        if (offset < 0) offset = 0;
        if (offset > 0) {
            for (std::size_t i = 0; i < line_count; ++i) {
                auto* ch = factory.get(line[i]);
                if (!ch) continue;
                const auto r = ch->get_rect();
                ch->set_pos(line_x[i] + offset, r.y);
            }
        }
        line_count = 0;
        line_w = 0;
        line_h = 0;
    };

    for (std::size_t i = 0; i < container.child_count(); ++i) {
        auto h = container.child_at(i);
        auto* ch = factory.get(h);
        if (!ch || !ch->is_visible()) continue;
        auto r = ch->get_rect();
        if (r.w <= 0 || r.h <= 0) continue;
        if (cursor_x != inner_x && (cursor_x + r.w) > max_x) {
            const int prev_line_h = line_h;
            flush_line();
            cursor_x = inner_x;
            cursor_y += prev_line_h + line_gap;
        }
        ch->set_pos(cursor_x, cursor_y);
        if (line_count < line.size()) {
            line[line_count] = h;
            line_x[line_count] = cursor_x;
            ++line_count;
        }
        if (line_w == 0) line_w = r.w;
        else line_w += gap + r.w;
        cursor_x += r.w + gap;
        if (r.h > line_h) line_h = r.h;
    }
    flush_line();
}

export
inline void apply_flow_layout(UiFactory& factory, ObjectBase& container) {
    apply_flow_layout(factory, container,
                      container.flow_gap(),
                      container.flow_line_gap(),
                      container.flow_padding());
}

export
inline void apply_grid_layout(UiFactory& factory, ObjectBase& container,
                              int cols, int cell_w, int cell_h,
                              int gap, int padding) {
    const auto rect = container.layout_rect();
    cols = (cols > 0) ? cols : 1;
    const int align_h = container.align_h();
    const int align_v = container.align_v();
    const int inner_w = rect.w - padding * 2;
    if (cell_w <= 0) {
        cell_w = (cols > 0) ? (inner_w - gap * (cols - 1)) / cols : 0;
        if (cell_w < 0) cell_w = 0;
    }

    int visible_count = 0;
    for (std::size_t i = 0; i < container.child_count(); ++i) {
        auto* ch = factory.get(container.child_at(i));
        if (!ch || !ch->is_visible()) continue;
        ++visible_count;
    }
    if (visible_count == 0) return;

    const int rows = (visible_count + cols - 1) / cols;
    constexpr int kMaxRows = 64;
    std::array<int, kMaxRows> row_heights{};
    std::array<int, kMaxRows + 1> row_offsets{};
    if (cell_h <= 0) {
        int index = 0;
        for (std::size_t i = 0; i < container.child_count(); ++i) {
            auto h = container.child_at(i);
            auto* ch = factory.get(h);
            if (!ch || !ch->is_visible()) continue;
            auto r = ch->get_rect();
            const int row = index / cols;
            if (row < kMaxRows && r.h > row_heights[row]) {
                row_heights[row] = r.h;
            }
            ++index;
        }
    } else {
        const int capped = (rows < kMaxRows) ? rows : kMaxRows;
        for (int i = 0; i < capped; ++i) {
            row_heights[i] = cell_h;
        }
    }

    const int use_rows = (rows < kMaxRows) ? rows : kMaxRows;
    int total_h = 0;
    if (cell_h > 0) {
        total_h = rows * cell_h + gap * (rows - 1);
    } else {
        for (int i = 0; i < use_rows; ++i) {
            total_h += row_heights[i];
        }
        total_h += gap * (rows - 1);
    }

    const int grid_w = cols * cell_w + gap * (cols - 1);
    int offset_x = 0;
    if (align_h == static_cast<int>(AlignH::Center)) {
        offset_x = (inner_w - grid_w) / 2;
    } else if (align_h == static_cast<int>(AlignH::End)) {
        offset_x = inner_w - grid_w;
    }
    if (offset_x < 0) offset_x = 0;

    int offset_y = 0;
    if (align_v == static_cast<int>(AlignV::Center)) {
        offset_y = (rect.h - padding * 2 - total_h) / 2;
    } else if (align_v == static_cast<int>(AlignV::End)) {
        offset_y = rect.h - padding * 2 - total_h;
    }
    if (offset_y < 0) offset_y = 0;

    for (int i = 0; i < use_rows; ++i) {
        row_offsets[i + 1] = row_offsets[i] + row_heights[i] + gap;
    }

    int index = 0;
    for (std::size_t i = 0; i < container.child_count(); ++i) {
        auto h = container.child_at(i);
        auto* ch = factory.get(h);
        if (!ch || !ch->is_visible()) continue;
        auto r = ch->get_rect();
        const int col = index % cols;
        const int row = index / cols;
        const int x = rect.x + padding + offset_x + col * (cell_w + gap);
        int y = rect.y + padding + offset_y;
        if (row < kMaxRows) {
            y += row_offsets[row];
        } else {
            y += row * ((cell_h > 0 ? cell_h : r.h) + gap);
        }
        if (cell_w > 0) r.w = cell_w;
        if (cell_h > 0) r.h = cell_h;
        ch->set_rect({x, y, r.w, r.h});
        ++index;
    }
}

export
inline void apply_grid_layout(UiFactory& factory, ObjectBase& container) {
    apply_grid_layout(factory, container,
                      container.grid_columns(),
                      container.grid_cell_width(),
                      container.grid_cell_height(),
                      container.grid_gap(),
                      container.grid_padding());
}

inline void apply_anchor_layout(UiFactory& factory, ObjectBase& container, Rect rect) {
    for (std::size_t i = 0; i < container.child_count(); ++i) {
        auto h = container.child_at(i);
        auto* ch = factory.get(h);
        if (!ch) continue;
        auto r = ch->get_rect();

        auto clamp_size = [](int value, int min_v, int max_v) {
            if (min_v > 0 && value < min_v) return min_v;
            if (max_v > 0 && value > max_v) return max_v;
            return value;
        };

        if (ch->has_percent_size()) {
            if (ch->percent_width() >= 0) {
                r.w = rect.w * ch->percent_width() / 100;
            }
            if (ch->percent_height() >= 0) {
                r.h = rect.h * ch->percent_height() / 100;
            }
        }

        const int max_w = ch->max_width();
        const int max_h = ch->max_height();
        const int min_w = ch->min_width();
        const int min_h = ch->min_height();
        r.w = clamp_size(r.w, min_w, max_w);
        r.h = clamp_size(r.h, min_h, max_h);

        if (!ch->has_anchor()) {
            const int align_h = ch->align_h();
            const int align_v = ch->align_v();
            int x = r.x;
            int y = r.y;
            int w = r.w;
            int h = r.h;

            if (align_h == static_cast<int>(AlignH::Stretch)) {
                w = clamp_size(rect.w, min_w, max_w);
                x = rect.x;
            } else if (align_h == static_cast<int>(AlignH::Center)) {
                x = rect.x + (rect.w - w) / 2;
            } else if (align_h == static_cast<int>(AlignH::End)) {
                x = rect.x + rect.w - w;
            } else {
                x = rect.x;
            }

            if (align_v == static_cast<int>(AlignV::Stretch)) {
                h = clamp_size(rect.h, min_h, max_h);
                y = rect.y;
            } else if (align_v == static_cast<int>(AlignV::Center)) {
                y = rect.y + (rect.h - h) / 2;
            } else if (align_v == static_cast<int>(AlignV::End)) {
                y = rect.y + rect.h - h;
            } else {
                y = rect.y;
            }

            r.x = x;
            r.y = y;
            r.w = w;
            r.h = h;
            ch->set_rect(r);
            continue;
        }

        const int left = ch->anchor_left();
        const int right = ch->anchor_right();
        const int top = ch->anchor_top();
        const int bottom = ch->anchor_bottom();

        const bool has_left = left >= 0;
        const bool has_right = right >= 0;
        const bool has_top = top >= 0;
        const bool has_bottom = bottom >= 0;

        const int area_x = rect.x + (has_left ? left : 0);
        const int area_y = rect.y + (has_top ? top : 0);
        const int area_w = rect.w - (has_left ? left : 0) - (has_right ? right : 0);
        const int area_h = rect.h - (has_top ? top : 0) - (has_bottom ? bottom : 0);

        if (has_left && has_right) {
            r.w = clamp_size(area_w, min_w, max_w);
        }
        if (has_top && has_bottom) {
            r.h = clamp_size(area_h, min_h, max_h);
        }

        if (has_left && has_right) {
            const int align_h = ch->align_h();
            if (align_h == static_cast<int>(AlignH::End)) {
                r.x = area_x + area_w - r.w;
            } else if (align_h == static_cast<int>(AlignH::Center)) {
                r.x = area_x + (area_w - r.w) / 2;
            } else {
                r.x = area_x;
            }
        } else if (has_left) {
            r.x = rect.x + left;
        } else if (has_right) {
            r.x = rect.x + rect.w - right - r.w;
        }

        if (has_top && has_bottom) {
            const int align_v = ch->align_v();
            if (align_v == static_cast<int>(AlignV::End)) {
                r.y = area_y + area_h - r.h;
            } else if (align_v == static_cast<int>(AlignV::Center)) {
                r.y = area_y + (area_h - r.h) / 2;
            } else {
                r.y = area_y;
            }
        } else if (has_top) {
            r.y = rect.y + top;
        } else if (has_bottom) {
            r.y = rect.y + rect.h - bottom - r.h;
        }

        ch->set_rect(r);
    }
}

export
inline void apply_anchor_layout(UiFactory& factory, ObjectBase& container) {
    apply_anchor_layout(factory, container, container.layout_rect());
}

export
inline void apply_constraint_layout(UiFactory& factory, ObjectBase& container, int padding = 0) {
    auto rect = container.layout_rect();
    if (padding > 0) {
        rect.x += padding;
        rect.y += padding;
        rect.w -= padding * 2;
        rect.h -= padding * 2;
        if (rect.w < 0) rect.w = 0;
        if (rect.h < 0) rect.h = 0;
    }
    apply_anchor_layout(factory, container, rect);
}

export
inline void apply_layout(UiFactory& factory, ObjectBase& container) {
    auto update_bounds = [&]() {
        Rect bounds{};
        bool has_bounds = false;
        for (std::size_t i = 0; i < container.child_count(); ++i) {
            auto h = container.child_at(i);
            auto* ch = factory.get(h);
            if (!ch || !ch->is_visible()) continue;
            const auto r = ch->get_rect();
            if (!has_bounds) {
                bounds = r;
                has_bounds = true;
            } else {
                const int left = (r.x < bounds.x) ? r.x : bounds.x;
                const int top = (r.y < bounds.y) ? r.y : bounds.y;
                const int right = ((r.x + r.w) > (bounds.x + bounds.w)) ? (r.x + r.w) : (bounds.x + bounds.w);
                const int bottom = ((r.y + r.h) > (bounds.y + bounds.h)) ? (r.y + r.h) : (bounds.y + bounds.h);
                bounds.x = left;
                bounds.y = top;
                bounds.w = right - left;
                bounds.h = bottom - top;
            }
        }
        container.set_children_bounds(bounds, has_bounds);
    };

    if (!container.has_layout_spec()) {
        apply_anchor_layout(factory, container);
        update_bounds();
        return;
    }

    const auto& spec = container.layout_spec();
    switch (spec.kind) {
    case ObjectBase::LayoutMode::Flex: {
        FlexLayoutConfig cfg{};
        cfg.flow = static_cast<FlexFlow>(spec.flow);
        cfg.main_align = static_cast<FlexAlign>(spec.main_align);
        cfg.cross_align = static_cast<FlexCrossAlign>(spec.cross_align);
        cfg.gap = spec.gap;
        cfg.padding = spec.padding;
        apply_flex_layout(factory, container, cfg);
        break;
    }
    case ObjectBase::LayoutMode::Flow:
        apply_flow_layout(factory, container, spec.gap, spec.line_gap, spec.padding);
        break;
    case ObjectBase::LayoutMode::Grid:
        apply_grid_layout(factory, container, spec.columns, spec.cell_w, spec.cell_h,
                          spec.grid_gap, spec.grid_padding);
        break;
    case ObjectBase::LayoutMode::Constraint:
        apply_constraint_layout(factory, container, spec.padding);
        break;
    case ObjectBase::LayoutMode::Custom:
        if (!apply_custom_layout(factory, container, spec)) {
            apply_anchor_layout(factory, container);
        }
        break;
    default:
        apply_anchor_layout(factory, container);
        break;
    }
    update_bounds();
}

module;
#include <cstddef>
export module charm.core.layout;

import charm.core.object;
import charm.core.factory;
import ui.common;

export
class Layout {
public:
    virtual ~Layout() = default;
    // 根据容器的尺寸和子节点的固有/优先尺寸，
    // 计算并设置每个子节点的 x,y,width,height
    virtual void apply(UiFactory& factory, ObjectBase& container) = 0;
};

export
class AbsoluteLayout : public Layout {
public:
    void apply(UiFactory&, ObjectBase&) override {
        // 不做任何改动，所有子对象保持自身 x,y
    }
};

export
class BoxLayout : public Layout {
public:
    enum class Direction { Horizontal, Vertical };

    BoxLayout(Direction dir, int spacing=0, int padding=0)
      : dir(dir), spacing(spacing), padding(padding) {}

    void apply(UiFactory& factory, ObjectBase& container) override {
        auto rect = container.get_rect();
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
    const auto rect = container.get_rect();
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
inline void apply_anchor_layout(UiFactory& factory, ObjectBase& container) {
    const auto rect = container.get_rect();
    for (std::size_t i = 0; i < container.child_count(); ++i) {
        auto h = container.child_at(i);
        auto* ch = factory.get(h);
        if (!ch) continue;
        auto r = ch->get_rect();

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

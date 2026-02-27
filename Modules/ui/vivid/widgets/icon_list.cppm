module;
export module charm.widgets.icon_list;

import charm.core.style;
import charm.gfx.color;
import charm.gfx.render;
import charm.widgets.list_view;
import charm.widgets.text;

using namespace ui::render;

export
struct IconListItem {
    const ImageView* icon{};
    const char* label{};
    rgba accent{0, 0, 0, 0};
};

// Icon list (ARM-2D icon_list inspired)
export
class IconList : public ListView {
public:
    IconList() {
        set_row_height(36);
        set_on_draw(&IconList::draw_item, this);
    }

    void set_items(const IconListItem* items, int count) noexcept {
        items_ = items;
        item_count_ = (count > 0) ? count : 0;
        set_item_count(item_count_);
    }

    const IconListItem* items() const noexcept { return items_; }
    int item_count() const noexcept { return item_count_; }

    void set_icon_size(int px) noexcept { icon_size_ = (px > 4) ? px : 4; }
    void set_icon_padding(int px) noexcept { icon_pad_ = (px >= 0) ? px : 0; }
    void set_icon_gap(int px) noexcept { icon_gap_ = (px >= 0) ? px : 0; }

private:
    static void draw_item(void* ctx, CanvasBase& cvs, const ListView::DrawInfo& info) noexcept {
        auto* self = static_cast<IconList*>(ctx);
        if (!self || !self->items_) return;
        if (info.index < 0 || info.index >= self->item_count_) return;

        const auto& item = self->items_[info.index];
        const auto& st = Theme::instance().get<ListView>();
        const int pad = st.metrics.padding;
        const int icon_size = self->icon_size_;
        const int icon_x = info.rect.x + pad + self->icon_pad_;
        const int icon_y = info.rect.y + (info.rect.h - icon_size) / 2;
        const Rect icon_rect{icon_x, icon_y, icon_size, icon_size};

        if (item.icon) {
            draw_image_scaled(cvs, icon_rect.x, icon_rect.y, icon_rect.w, icon_rect.h, *item.icon);
        } else {
            const rgba fill = item.accent.a ? item.accent : st.colors.border_color;
            draw_round_rect(cvs, icon_rect.x, icon_rect.y, icon_rect.w, icon_rect.h, 4, fill, true);
            draw_round_rect(cvs, icon_rect.x, icon_rect.y, icon_rect.w, icon_rect.h, 4, st.colors.border_color, false);
        }

        Rect text_rect = info.rect;
        const int text_x = icon_rect.x + icon_rect.w + self->icon_gap_;
        text_rect.x = text_x;
        text_rect.w = info.rect.x + info.rect.w - text_x - pad;
        if (text_rect.w <= 0) return;
        draw_text_box(cvs, text_rect, item.label ? item.label : "",
                      st.colors.font_color, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Center,
                      TextWrap::None, TextEllipsis::End);
    }

    const IconListItem* items_{nullptr};
    int item_count_{0};
    int icon_size_{20};
    int icon_pad_{6};
    int icon_gap_{8};
};

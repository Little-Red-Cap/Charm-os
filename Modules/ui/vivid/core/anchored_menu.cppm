module;
#include <algorithm>
#include <array>
#include <cstddef>

export module charm.ui.scene.anchored_menu;

import charm.core.geometry;
import charm.core.handle;
import charm.core.style;
import charm.ui.scene.builder_support;

export namespace ui::scene {
    inline constexpr std::size_t kAnchoredMenuMaxItems = 6;
    inline constexpr int kAnchoredMenuOffscreen = -4096;

    struct AnchoredMenuSpec {
        int width{196};
        int padding_left{12};
        int padding_right{12};
        int padding_top{12};
        int padding_bottom{16};
        int title_height{18};
        int title_bottom_gap{10};
        int item_height{34};
        int item_gap{8};
        int anchor_right_inset{12};
        int anchor_top_offset{6};
        int viewport_margin{8};
        StyleClassId card_style_class{kStyleClassInvalid};
        StyleClassId item_style_class{kStyleClassInvalid};
        StylePatch scrim_patch{};
        bool apply_scrim_patch{false};
    };

    struct AnchoredMenuHandles {
        WidgetHandle scrim{};
        WidgetHandle card{};
        WidgetHandle title{};
        std::array<WidgetHandle, kAnchoredMenuMaxItems> items{};

        [[nodiscard]] WidgetHandle item(std::size_t index) const noexcept {
            return index < items.size() ? items[index] : WidgetHandle{};
        }
    };

    struct AnchoredMenuItemSpec {
        const char* text{nullptr};
        bool visible{true};
    };

    struct AnchoredMenuShowSpec {
        Rect anchor{};
        Rect viewport{};
        const char* title{nullptr};
        const AnchoredMenuItemSpec* items{nullptr};
        std::size_t item_count{0};
    };

    inline StylePatch default_anchored_menu_scrim_patch() noexcept {
        StylePatch patch{};
        patch.has_bg_color = true;
        patch.bg_color = {0, 0, 0, 28};
        patch.has_border_color = true;
        patch.border_color = {0, 0, 0, 0};
        patch.has_font_color = true;
        patch.font_color = {0, 0, 0, 0};
        patch.has_shadow_enabled = true;
        patch.shadow_enabled = false;
        return patch;
    }

    inline std::size_t anchored_menu_item_count(std::size_t count) noexcept {
        return count < kAnchoredMenuMaxItems ? count : kAnchoredMenuMaxItems;
    }

    inline std::size_t anchored_menu_visible_item_count(const AnchoredMenuItemSpec* items,
                                                        std::size_t count) noexcept {
        if (!items) return 0;
        const std::size_t cap = anchored_menu_item_count(count);
        std::size_t visible = 0;
        for (std::size_t i = 0; i < cap; ++i) {
            if (items[i].visible) ++visible;
        }
        return visible;
    }

    inline int anchored_menu_item_index(const AnchoredMenuHandles& handles,
                                        WidgetHandle target) noexcept {
        if (!target) return -1;
        for (std::size_t i = 0; i < handles.items.size(); ++i) {
            if (handles.items[i] == target) return static_cast<int>(i);
        }
        return -1;
    }

    inline int anchored_menu_first_visible_index(const AnchoredMenuItemSpec* items,
                                                 std::size_t count) noexcept {
        if (!items) return -1;
        const std::size_t cap = anchored_menu_item_count(count);
        for (std::size_t i = 0; i < cap; ++i) {
            if (items[i].visible) return static_cast<int>(i);
        }
        return -1;
    }

    inline int anchored_menu_last_visible_index(const AnchoredMenuItemSpec* items,
                                                std::size_t count) noexcept {
        if (!items) return -1;
        const std::size_t cap = anchored_menu_item_count(count);
        for (std::size_t i = cap; i > 0; --i) {
            if (items[i - 1].visible) return static_cast<int>(i - 1);
        }
        return -1;
    }

    inline int anchored_menu_next_visible_index(const AnchoredMenuItemSpec* items,
                                                std::size_t count,
                                                int current,
                                                int delta) noexcept {
        if (!items) return -1;
        const std::size_t cap = anchored_menu_item_count(count);
        if (cap == 0) return -1;
        if (anchored_menu_visible_item_count(items, count) == 0) return -1;

        const int step = (delta < 0) ? -1 : 1;
        if (current < 0 || current >= static_cast<int>(cap) || !items[current].visible) {
            return step > 0
                ? anchored_menu_first_visible_index(items, count)
                : anchored_menu_last_visible_index(items, count);
        }

        int index = current;
        for (std::size_t attempts = 0; attempts < cap; ++attempts) {
            index += step;
            if (index < 0) index = static_cast<int>(cap) - 1;
            if (index >= static_cast<int>(cap)) index = 0;
            if (items[index].visible) return index;
        }
        return current;
    }

    inline int anchored_menu_focused_index(SceneAccess& access,
                                           const AnchoredMenuHandles& handles) noexcept {
        return anchored_menu_item_index(handles, access.input_focused());
    }

    inline void set_anchored_menu_focus(SceneAccess& access,
                                        const AnchoredMenuHandles& handles,
                                        int index) noexcept {
        if (!access.valid()) return;
        for (std::size_t i = 0; i < handles.items.size(); ++i) {
            const WidgetHandle item = handles.items[i];
            if (!item) continue;
            access.set_focused(item, static_cast<int>(i) == index);
        }
    }

    inline int anchored_menu_card_height(const AnchoredMenuSpec& spec,
                                         std::size_t item_count,
                                         bool show_title = true) noexcept {
        const std::size_t visible_items = anchored_menu_item_count(item_count);
        int height = spec.padding_top + spec.padding_bottom;
        if (show_title) {
            height += spec.title_height;
            if (visible_items > 0) height += spec.title_bottom_gap;
        }
        if (visible_items > 0) {
            height += static_cast<int>(visible_items) * spec.item_height;
            height += static_cast<int>(visible_items - 1) * spec.item_gap;
        }
        return height;
    }

    inline AnchoredMenuHandles build_anchored_menu(SceneBuilder& builder,
                                                   WidgetHandle parent,
                                                   std::size_t item_capacity,
                                                   const AnchoredMenuSpec& spec = {}) noexcept {
        AnchoredMenuHandles out{};
        const std::size_t capacity = anchored_menu_item_count(item_capacity);

        out.scrim = builder.create_button_static("");
        builder.set_rect(out.scrim, {0, 0, 0, 0});
        builder.set_style_override(out.scrim,
                                   spec.apply_scrim_patch ? spec.scrim_patch
                                                          : default_anchored_menu_scrim_patch());

        out.card = builder.create_container();
        builder.set_rect(out.card,
                         {kAnchoredMenuOffscreen, kAnchoredMenuOffscreen,
                          spec.width, anchored_menu_card_height(spec, capacity)});
        if (spec.card_style_class != kStyleClassInvalid) {
            builder.set_style_class(out.card, spec.card_style_class);
        }
        builder.set_hit_testable(out.card, false);

        out.title = builder.create_label_static("");
        builder.set_rect(out.title,
                         {spec.padding_left,
                          spec.padding_top,
                          spec.width - spec.padding_left - spec.padding_right,
                          spec.title_height});
        builder.set_label_align(out.title, TextAlignH::Left, TextAlignV::Center);
        builder.set_hit_testable(out.title, false);

        int item_y = spec.padding_top + spec.title_height + spec.title_bottom_gap;
        for (std::size_t i = 0; i < capacity; ++i) {
            out.items[i] = builder.create_button_static("");
            builder.set_rect(out.items[i],
                             {spec.padding_left,
                              item_y,
                              spec.width - spec.padding_left - spec.padding_right,
                              spec.item_height});
            builder.set_label_align(out.items[i], TextAlignH::Left, TextAlignV::Center);
            if (spec.item_style_class != kStyleClassInvalid) {
                builder.set_style_class(out.items[i], spec.item_style_class);
            }
            item_y += spec.item_height + spec.item_gap;
        }

        if (parent) {
            builder.link(parent, out.scrim);
            builder.link(parent, out.card);
            builder.link(out.card, out.title);
            for (std::size_t i = 0; i < capacity; ++i) {
                builder.link(out.card, out.items[i]);
            }
        }
        return out;
    }

    inline void hide_anchored_menu(SceneAccess& access,
                                   const AnchoredMenuHandles& handles,
                                   const AnchoredMenuSpec& spec = {}) noexcept {
        if (!access.valid()) return;
        if (handles.scrim) {
            access.set_visible(handles.scrim, false);
            access.set_rect(handles.scrim, {0, 0, 0, 0});
        }
        if (handles.card) {
            access.set_visible(handles.card, false);
            access.set_rect(handles.card,
                            {kAnchoredMenuOffscreen, kAnchoredMenuOffscreen,
                             spec.width, anchored_menu_card_height(spec, handles.items.size())});
        }
        if (handles.title) access.set_visible(handles.title, false);
        for (const auto item : handles.items) {
            if (item) access.set_visible(item, false);
        }
    }

    inline void show_anchored_menu(SceneAccess& access,
                                   const AnchoredMenuHandles& handles,
                                   const AnchoredMenuShowSpec& show,
                                   const AnchoredMenuSpec& spec = {}) noexcept {
        if (!access.valid()) return;
        const std::size_t visible_items = anchored_menu_visible_item_count(show.items, show.item_count);
        if (visible_items == 0) {
            hide_anchored_menu(access, handles, spec);
            return;
        }

        const bool show_title = show.title && show.title[0] != '\0';
        const int card_h = anchored_menu_card_height(spec, visible_items, show_title);
        const int item_w = spec.width - spec.padding_left - spec.padding_right;
        const int title_w = spec.width - spec.padding_left - spec.padding_right;

        int menu_x = show.anchor.x + show.anchor.w - spec.width - spec.anchor_right_inset;
        int menu_y = show.anchor.y + spec.anchor_top_offset;
        if (show.viewport.w > 0 && show.viewport.h > 0) {
            const int min_x = show.viewport.x + spec.viewport_margin;
            const int max_x = show.viewport.x + show.viewport.w - spec.width - spec.viewport_margin;
            const int min_y = show.viewport.y + spec.viewport_margin;
            const int max_y = show.viewport.y + show.viewport.h - card_h - spec.viewport_margin;
            menu_x = (max_x >= min_x) ? std::clamp(menu_x, min_x, max_x) : min_x;
            menu_y = (max_y >= min_y) ? std::clamp(menu_y, min_y, max_y) : min_y;
        }

        if (handles.scrim) {
            access.set_rect(handles.scrim, show.viewport);
            access.set_visible(handles.scrim, true);
        }
        if (handles.card) {
            access.set_rect(handles.card, {menu_x, menu_y, spec.width, card_h});
            access.set_visible(handles.card, true);
        }
        if (handles.title) {
            access.set_text(handles.title, show_title ? show.title : "");
            access.set_rect(handles.title,
                            {spec.padding_left, spec.padding_top, title_w, spec.title_height});
            access.set_visible(handles.title, show_title);
        }

        int item_y = spec.padding_top + (show_title ? (spec.title_height + spec.title_bottom_gap) : 0);
        for (std::size_t i = 0; i < handles.items.size(); ++i) {
            const WidgetHandle item = handles.items[i];
            if (!item) continue;
            const bool should_show = show.items
                && i < anchored_menu_item_count(show.item_count)
                && show.items[i].visible;
            if (should_show) {
                const char* text = show.items[i].text ? show.items[i].text : "";
                access.set_text(item, text);
                access.set_rect(item,
                                {spec.padding_left, item_y, item_w, spec.item_height});
                access.set_visible(item, true);
                item_y += spec.item_height + spec.item_gap;
            } else {
                access.set_visible(item, false);
            }
        }
    }
}

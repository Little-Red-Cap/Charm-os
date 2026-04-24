export module charm.ui.scene.list_card_header;

export import charm.core.geometry;

export namespace ui::scene {
    struct ListCardHeaderSpec {
        int title_left_gap{};
        int title_top_gap{};
        int title_w_reduce{};
        int title_h{};
        int action_w{};
        int action_h{};
        int action_right_gap{};
        int action_top_offset{};
        int path_top_gap{};
        int path_side_gap{};
        int path_h{};
        int path_bg_h_extra{};
        int body_top_gap{};
        int body_side_gap{};
        int body_bottom_gap{};
        int scroll_w{};
        int scroll_right_gap{};
        int hint_left_gap{};
        int hint_right_gap{};
        int hint_bottom_gap{};
        int hint_h{};
    };

    struct ListCardHeaderLayout {
        Rect title_rect{};
        Rect action_rect{};
        Rect path_rect{};
        Rect body_rect{};
        Rect scroll_rect{};
        Rect hint_rect{};
    };

    constexpr ListCardHeaderLayout make_list_card_header_layout(const Rect& card_rect,
                                                                const ListCardHeaderSpec& spec) noexcept {
        const int title_y = card_rect.y + spec.title_top_gap;
        const int path_y = title_y + spec.title_h + spec.path_top_gap;
        const Rect path_rect{
            card_rect.x + spec.path_side_gap,
            path_y,
            card_rect.w - spec.path_side_gap * 2,
            spec.path_h + spec.path_bg_h_extra,
        };
        const int body_y = card_rect.y + spec.body_top_gap;
        const int body_h = card_rect.h - spec.body_top_gap - spec.body_bottom_gap;
        return ListCardHeaderLayout{
            .title_rect = Rect{
                card_rect.x + spec.title_left_gap,
                title_y,
                card_rect.w - spec.title_w_reduce,
                spec.title_h,
            },
            .action_rect = Rect{
                card_rect.x + card_rect.w - spec.action_right_gap,
                title_y + spec.action_top_offset,
                spec.action_w,
                spec.action_h,
            },
            .path_rect = path_rect,
            .body_rect = Rect{
                card_rect.x + spec.body_side_gap,
                body_y,
                card_rect.w - spec.body_side_gap * 2,
                body_h,
            },
            .scroll_rect = Rect{
                card_rect.x + card_rect.w - spec.scroll_right_gap,
                body_y,
                spec.scroll_w,
                body_h,
            },
            .hint_rect = Rect{
                card_rect.x + spec.hint_left_gap,
                card_rect.y + card_rect.h - spec.hint_bottom_gap,
                card_rect.w - spec.hint_right_gap,
                spec.hint_h,
            },
        };
    }
}

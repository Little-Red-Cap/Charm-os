module;

export module charm.ui.scene.path_bar;

export import charm.core.geometry;

export namespace ui::scene {
    struct PathBarSpec {
        int left_gap{};
        int right_gap{};
        int icon_top_gap{};
        int label_top_gap{};
        int icon_size{};
        int icon_gap{};
        int content_h{};
    };

    struct PathBarLayout {
        Rect bar_rect{};
        Rect icon_rect{};
        Rect label_rect{};
    };

    constexpr PathBarLayout make_path_bar_layout(const Rect& bar_rect,
                                                 const PathBarSpec& spec) noexcept {
        return PathBarLayout{
            .bar_rect = bar_rect,
            .icon_rect = Rect{
                bar_rect.x + spec.left_gap,
                bar_rect.y + spec.icon_top_gap,
                spec.icon_size,
                spec.icon_size,
            },
            .label_rect = Rect{
                bar_rect.x + spec.left_gap + spec.icon_size + spec.icon_gap,
                bar_rect.y + spec.label_top_gap,
                bar_rect.w - spec.left_gap - spec.icon_size - spec.icon_gap - spec.right_gap,
                spec.content_h,
            },
        };
    }
}

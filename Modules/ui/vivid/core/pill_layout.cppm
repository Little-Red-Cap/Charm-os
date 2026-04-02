export module charm.ui.scene.pill;

export import charm.core.geometry;

export namespace ui::scene {
    struct PillContentSpec {
        int left_gap{};
        int right_gap{};
        int icon_top_gap{};
        int label_top_gap{};
        int icon_size{};
        int icon_gap{};
        int content_h{};
    };

    constexpr Rect pill_icon_rect(const Rect& pill_rect,
                                  const PillContentSpec& spec) noexcept {
        return Rect{
            pill_rect.x + spec.left_gap,
            pill_rect.y + spec.icon_top_gap,
            spec.icon_size,
            spec.icon_size,
        };
    }

    constexpr Rect pill_label_rect(const Rect& pill_rect,
                                   const PillContentSpec& spec) noexcept {
        return Rect{
            pill_rect.x + spec.left_gap + spec.icon_size + spec.icon_gap,
            pill_rect.y + spec.label_top_gap,
            pill_rect.w - spec.left_gap - spec.icon_size - spec.icon_gap - spec.right_gap,
            spec.content_h,
        };
    }
}

export module charm.ui.scene.page_header;

export import charm.core.geometry;

export namespace ui::scene {
    struct HeaderBoxSpec {
        int width{};
        int height{};
        int top_gap{};
    };

    constexpr Rect page_header_left_rect(const Rect& bar_rect,
                                         const HeaderBoxSpec& spec,
                                         int left_gap = 0) noexcept {
        return Rect{
            bar_rect.x + left_gap,
            bar_rect.y + spec.top_gap,
            spec.width,
            spec.height,
        };
    }

    constexpr Rect page_header_right_rect(const Rect& bar_rect,
                                          const HeaderBoxSpec& spec,
                                          int right_gap = 0) noexcept {
        return Rect{
            bar_rect.x + bar_rect.w - right_gap,
            bar_rect.y + spec.top_gap,
            spec.width,
            spec.height,
        };
    }

    constexpr Rect page_header_title_rect(const Rect& bar_rect,
                                          int left_gap,
                                          int top_gap,
                                          int right_gap,
                                          int height) noexcept {
        return Rect{
            bar_rect.x + left_gap,
            bar_rect.y + top_gap,
            bar_rect.w - right_gap,
            height,
        };
    }
}

module;

export module charm.ui.scene.top_bar;

export import charm.core.geometry;

export namespace ui::scene {
    struct TopBarTitleSpec {
        int left_gap{};
        int top_gap{};
        int right_gap{};
        int height{};
    };

    struct TopBarButtonSpec {
        int width{};
        int height{};
        int top_gap{};
    };

    struct TopBarLayout {
        Rect bar_rect{};
        Rect left_rect{};
        Rect title_rect{};
        Rect right_primary_rect{};
        Rect right_secondary_rect{};
    };

    constexpr TopBarLayout make_top_bar_layout(const Rect& bar_rect,
                                               const TopBarButtonSpec* left_button,
                                               const TopBarTitleSpec* title,
                                               const TopBarButtonSpec* right_primary,
                                               int right_primary_gap,
                                               const TopBarButtonSpec* right_secondary = nullptr,
                                               int right_secondary_gap = 0) noexcept {
        TopBarLayout out{};
        out.bar_rect = bar_rect;
        if (left_button) {
            out.left_rect = Rect{
                bar_rect.x,
                bar_rect.y + left_button->top_gap,
                left_button->width,
                left_button->height,
            };
        }
        if (title) {
            out.title_rect = Rect{
                bar_rect.x + title->left_gap,
                bar_rect.y + title->top_gap,
                bar_rect.w - title->right_gap,
                title->height,
            };
        }
        if (right_primary) {
            out.right_primary_rect = Rect{
                bar_rect.x + bar_rect.w - right_primary_gap,
                bar_rect.y + right_primary->top_gap,
                right_primary->width,
                right_primary->height,
            };
        }
        if (right_secondary) {
            out.right_secondary_rect = Rect{
                bar_rect.x + bar_rect.w - right_secondary_gap,
                bar_rect.y + right_secondary->top_gap,
                right_secondary->width,
                right_secondary->height,
            };
        }
        return out;
    }
}

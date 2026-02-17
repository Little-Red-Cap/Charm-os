// gui.theme.cppm
// UI theme defaults shared by library and examples.

module;

export module gui.theme;

import gui.font;
import gui.font5x7_min;

export namespace gui::theme {
    struct ThemeSpec {
        const gui::Font* font_default{nullptr};
        int pad_xs{2};
        int pad_sm{3};
        int pad_md{4};
        int header_h{12};
        int footer_h{9};
        int list_pad{2};
        int list_item_h{14};
        int list_gap{3};
    };

    inline constexpr ThemeSpec kThemeComfort{
        &gui::kFont5x7,
        2, 3, 4,
        12, 9,
        0, 14, 3
    };

    inline constexpr ThemeSpec kThemeCompact{
        &gui::kFont5x7,
        1, 1, 2,
        12, 9,
        0, 10, 0
    };

    inline const ThemeSpec* gTheme = &kThemeCompact;

    inline const ThemeSpec& current() noexcept { return *gTheme; }

    inline void set_current(const bool compact) noexcept
    {
        gTheme = compact ? &kThemeCompact : &kThemeComfort;
    }
} // namespace gui::theme

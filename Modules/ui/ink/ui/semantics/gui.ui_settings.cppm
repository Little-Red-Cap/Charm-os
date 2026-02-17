// gui.ui_settings.cppm
// UI settings shared across render/animation components.

module;
#include <cstdint>

export module gui.ui_settings;

import gui.motion;

export namespace gui::ui {

    enum class Toggle : std::uint8_t {
        Off = 0,
        On = 1,
    };

    [[nodiscard]] inline bool is_on(Toggle v) noexcept { return v == Toggle::On; }
    [[nodiscard]] inline Toggle toggled(Toggle v) noexcept { return (v == Toggle::On) ? Toggle::Off : Toggle::On; }

    struct ListChromeStyle {
        std::int16_t title_dx{2};
        std::int16_t title_dy{2};
        bool show_title{true};
    };

    struct UiSettings {
        Toggle anim_enabled{Toggle::On};
        Toggle fps_overlay{Toggle::Off};
        Toggle theme_compact{Toggle::On};
        Toggle theme_invert{Toggle::Off};
        ListChromeStyle list_chrome{};
        gui::motion::AnimProfile anim{};
        gui::motion::AnimPreset anim_preset{gui::motion::AnimPreset::Default};
    };

} // namespace gui::ui

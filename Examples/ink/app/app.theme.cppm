// app.theme.cppm
// Example app theme wrapper.

module;
export module app.theme;

import gui.theme;

export namespace app::theme {
    using gui::theme::ThemeSpec;
    using gui::theme::kThemeComfort;
    using gui::theme::kThemeCompact;

    inline const ThemeSpec& current() noexcept { return gui::theme::current(); }

    inline void set_current(const bool compact) noexcept
    {
        gui::theme::set_current(compact);
    }
}

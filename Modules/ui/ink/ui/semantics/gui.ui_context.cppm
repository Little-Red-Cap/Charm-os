//
// UI context: read-only semantics + theme + time, with a single mutable viewport slot.
//

module;
#include <cstdint>
export module gui.ui_context;

import gui.core;
import gui.list_view;
import gui.ui_semantics;
import gui.theme;
import gui.ui_settings;

export namespace gui::ui {

    struct UiContext {
        UiSemantics* sem{nullptr};
        const gui::theme::ThemeSpec* theme{nullptr};
        const gui::ui::UiSettings* settings{nullptr};
        std::uint32_t now_ms{0};
        gui::ListViewport* viewport{nullptr}; // Only viewport may be mutated by UI code.
        gui::Rect clip{};
    };

} // namespace gui::ui

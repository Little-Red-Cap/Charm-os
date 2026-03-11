module;
#include <cstdint>
#include <optional>

// lifecycle: internal bridge; remove after ui_semantics/input_adapter merge is complete.
export module gui.ui_semantics_bridge;

import gui.ui_input_adapter;
import gui.ui_semantics;
import input.nav;
import input.raw_event;

export namespace gui::ui {
    struct SemanticsBridge {
        std::optional<input::Intent> intent{};
        input::NavResult nav{};
        UiSemantics next{};
    };

    [[nodiscard]] inline SemanticsBridge reduce_from_raw(const UiSemantics& prev,
                                                         const input::RawInputEvent& ev) noexcept
    {
        SemanticsBridge out{};
        out.intent = intent_from_raw(ev);
        out.nav = input::nav_from_intent(out.intent);
        out.next = reduce_semantics(prev, out.intent);
        return out;
    }

    [[nodiscard]] inline SemanticsBridge reduce_from_intent(const UiSemantics& prev,
                                                            const std::optional<input::Intent>& it) noexcept
    {
        SemanticsBridge out{};
        out.intent = it;
        out.nav = input::nav_from_intent(out.intent);
        out.next = reduce_semantics(prev, out.intent);
        return out;
    }
} // namespace gui::ui

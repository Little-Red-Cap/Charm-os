//
// Created by Joho on 2026/02/18.
//

module;
#include <cstdint>
#include <optional>

export module gui.ui_input_adapter;

import input.raw_event;
import input.intent;

export namespace gui::ui {
    [[nodiscard]] inline std::optional<input::Intent> intent_from_raw(const input::RawInputEvent& ev) noexcept
    {
        using input::IntentType;
        using input::RawInputEventType;

        switch (ev.type) {
        case RawInputEventType::Button:
            if (!ev.pressed) return std::nullopt;
            switch (ev.button) {
            case input::Button::Up:
                return input::Intent{.type = IntentType::NavPrev, .a = 0, .b = 0, .ms = ev.ms};
            case input::Button::Down:
                return input::Intent{.type = IntentType::NavNext, .a = 0, .b = 0, .ms = ev.ms};
            case input::Button::Enter:
                return input::Intent{.type = IntentType::Activate, .a = 0, .b = 0, .ms = ev.ms};
            case input::Button::Back:
                return input::Intent{.type = IntentType::Back, .a = 0, .b = 0, .ms = ev.ms};
            }
            break;
        case RawInputEventType::Encoder:
            if (ev.encoder_delta == 0) return std::nullopt;
            return input::Intent{.type = IntentType::Adjust, .a = ev.encoder_delta, .b = 0, .ms = ev.ms};
        case RawInputEventType::Pointer:
            switch (ev.pointer_action) {
            case input::PointerAction::Down:
                return input::Intent{.type = IntentType::PointerDown, .a = ev.pointer.x, .b = ev.pointer.y, .ms = ev.ms};
            case input::PointerAction::Move:
                return input::Intent{.type = IntentType::PointerMove, .a = ev.pointer.x, .b = ev.pointer.y, .ms = ev.ms};
            case input::PointerAction::Up:
                return input::Intent{.type = IntentType::PointerUp, .a = ev.pointer.x, .b = ev.pointer.y, .ms = ev.ms};
            }
            break;
        case RawInputEventType::Axis:
        case RawInputEventType::None:
        default:
            break;
        }
        return std::nullopt;
    }
} // namespace gui::ui

module;
#include <optional>

export module charm.core.input_adapter;

import charm.core.event;
import input.raw;
import input.raw_event;

export namespace input::adapter {
    inline Event::Key map_button_key(Button b) noexcept {
        switch (b) {
        case Button::Up: return Event::Key::Up;
        case Button::Down: return Event::Key::Down;
        case Button::Enter: return Event::Key::Enter;
        case Button::Back: return Event::Key::Backspace;
        default: return Event::Key::Unknown;
        }
    }

    inline std::optional<Event> to_vivid_event(const RawInputEvent& raw) noexcept {
        switch (raw.type) {
        case RawInputEventType::Pointer: {
            const auto& p = raw.pointer;
            switch (raw.pointer_action) {
            case PointerAction::Down:
                return Event::mouse(Event::Type::MouseDown, p.x, p.y, 0);
            case PointerAction::Move:
                return Event::mouse(Event::Type::MouseMove, p.x, p.y, 0);
            case PointerAction::Up:
                return Event::mouse(Event::Type::MouseUp, p.x, p.y, 0);
            default:
                return std::nullopt;
            }
        }
        case RawInputEventType::Button: {
            const auto key = map_button_key(raw.button);
            if (key == Event::Key::Unknown) return std::nullopt;
            const auto type = raw.pressed ? Event::Type::KeyDown : Event::Type::KeyUp;
            return Event::key(type, key);
        }
        case RawInputEventType::Encoder: {
            if (raw.encoder_delta == 0) return std::nullopt;
            return Event::wheel(0, 0, raw.encoder_delta);
        }
        case RawInputEventType::Axis:
        case RawInputEventType::None:
        default:
            return std::nullopt;
        }
    }
} // namespace input::adapter

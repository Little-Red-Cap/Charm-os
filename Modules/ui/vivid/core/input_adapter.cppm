module;
#include <optional>

export module charm.core.input_adapter;

import charm.core.event;
import input.raw;
import input.raw_event;
import input.nav;

export namespace input::adapter {
    enum class EncoderMap : std::uint8_t {
        NavKey = 0,
        MouseWheel = 1
    };

    inline EncoderMap& encoder_map_ref() noexcept {
        static EncoderMap mode = EncoderMap::NavKey;
        return mode;
    }

    inline void set_encoder_map(EncoderMap mode) noexcept {
        encoder_map_ref() = mode;
    }

    inline EncoderMap encoder_map() noexcept {
        return encoder_map_ref();
    }

    inline std::optional<Intent> intent_from_button(Button b) noexcept {
        switch (b) {
        case Button::Up:
            return Intent{.type = IntentType::NavPrev, .a = 0, .b = 0, .ms = 0};
        case Button::Down:
            return Intent{.type = IntentType::NavNext, .a = 0, .b = 0, .ms = 0};
        case Button::Enter:
            return Intent{.type = IntentType::Activate, .a = 0, .b = 0, .ms = 0};
        case Button::Back:
            return Intent{.type = IntentType::Back, .a = 0, .b = 0, .ms = 0};
        default:
            break;
        }
        return std::nullopt;
    }

    inline Event::Key key_from_nav(const NavResult& nav) noexcept {
        if (nav.activated) return Event::Key::Enter;
        if (nav.back) return Event::Key::Backspace;
        if (nav.focus_delta < 0) return Event::Key::Up;
        if (nav.focus_delta > 0) return Event::Key::Down;
        return Event::Key::Unknown;
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
            const auto intent = intent_from_button(raw.button);
            const auto nav = nav_from_intent(intent);
            const auto key = key_from_nav(nav);
            if (key == Event::Key::Unknown) return std::nullopt;
            const auto type = raw.pressed ? Event::Type::KeyDown : Event::Type::KeyUp;
            return Event::key(type, key);
        }
        case RawInputEventType::Encoder: {
            if (raw.encoder_delta == 0) return std::nullopt;
            if (encoder_map() == EncoderMap::MouseWheel) {
                return Event::wheel(0, 0, raw.encoder_delta);
            }
            const Intent it{.type = IntentType::Adjust, .a = raw.encoder_delta, .b = 0, .ms = raw.ms};
            const auto nav = nav_from_intent(it);
            const auto key = key_from_nav(nav);
            if (key == Event::Key::Unknown) return std::nullopt;
            return Event::key(Event::Type::KeyDown, key);
        }
        case RawInputEventType::Axis:
        case RawInputEventType::None:
        default:
            return std::nullopt;
        }
    }
} // namespace input::adapter

module;
#include <cstdint>
#include <optional>

export module ui.input_adapter;

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

    inline Event::Key key_from_button(Button b) noexcept {
        switch (b) {
        case Button::Up: return Event::Key::Up;
        case Button::Down: return Event::Key::Down;
        case Button::Enter: return Event::Key::Enter;
        case Button::Back: return Event::Key::Backspace;
        default: return Event::Key::Unknown;
        }
    }

    inline Event::Key key_from_nav(const NavResult& nav) noexcept {
        if (nav.activated) return Event::Key::Enter;
        if (nav.back) return Event::Key::Backspace;
        if (nav.focus_delta < 0) return Event::Key::Up;
        if (nav.focus_delta > 0) return Event::Key::Down;
        return Event::Key::Unknown;
    }

    inline std::optional<Intent> intent_from_raw(const RawInputEvent& raw) noexcept {
        switch (raw.type) {
        case RawInputEventType::Pointer: {
            const auto& p = raw.pointer;
            switch (raw.pointer_action) {
            case PointerAction::Down:
                return Intent{.type = IntentType::PointerDown, .a = p.x, .b = p.y, .ms = raw.ms};
            case PointerAction::Move:
                return Intent{.type = IntentType::PointerMove, .a = p.x, .b = p.y, .ms = raw.ms};
            case PointerAction::Up:
                return Intent{.type = IntentType::PointerUp, .a = p.x, .b = p.y, .ms = raw.ms};
            default:
                return std::nullopt;
            }
        }
        case RawInputEventType::Button: {
            if (!raw.pressed) return std::nullopt;
            return intent_from_button(raw.button);
        }
        case RawInputEventType::Encoder: {
            if (raw.encoder_delta == 0) return std::nullopt;
            return Intent{.type = IntentType::Adjust, .a = raw.encoder_delta, .b = 0, .ms = raw.ms};
        }
        case RawInputEventType::Axis:
        case RawInputEventType::None:
        default:
            return std::nullopt;
        }
    }

    struct SemanticBridge {
        std::optional<Intent> intent{};
        NavResult nav{};
        std::optional<Event> event{};
    };

    inline SemanticBridge bridge_from_raw(const RawInputEvent& raw) noexcept {
        SemanticBridge out{};
        out.intent = intent_from_raw(raw);
        out.nav = nav_from_intent(out.intent);

        switch (raw.type) {
        case RawInputEventType::Pointer: {
            const auto& p = raw.pointer;
            switch (raw.pointer_action) {
            case PointerAction::Down:
                out.event = Event::mouse(Event::Type::MouseDown, p.x, p.y, 0, raw.ms);
                break;
            case PointerAction::Move:
                out.event = Event::mouse(Event::Type::MouseMove, p.x, p.y, 0, raw.ms);
                break;
            case PointerAction::Up:
                out.event = Event::mouse(Event::Type::MouseUp, p.x, p.y, 0, raw.ms);
                break;
            default:
                break;
            }
            break;
        }
        case RawInputEventType::Button: {
            const auto key = raw.pressed ? key_from_nav(out.nav) : key_from_button(raw.button);
            if (key != Event::Key::Unknown) {
                const auto type = raw.pressed ? Event::Type::KeyDown : Event::Type::KeyUp;
                out.event = Event::key(type, key, raw.ms);
            }
            break;
        }
        case RawInputEventType::Encoder: {
            if (raw.encoder_delta == 0) break;
            if (encoder_map() == EncoderMap::MouseWheel) {
                out.event = Event::wheel(0, 0, raw.encoder_delta, raw.ms);
                break;
            }
            const auto key = key_from_nav(out.nav);
            if (key != Event::Key::Unknown) {
                out.event = Event::key(Event::Type::KeyDown, key, raw.ms);
            }
            break;
        }
        case RawInputEventType::Axis:
        case RawInputEventType::None:
        default:
            break;
        }

        return out;
    }

    inline std::optional<Event> to_vivid_event(const RawInputEvent& raw) noexcept {
        return bridge_from_raw(raw).event;
    }
} // namespace input::adapter

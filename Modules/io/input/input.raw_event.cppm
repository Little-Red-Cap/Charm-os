//
// Created by Joho on 2026/02/18.
//

module;
#include <cstdint>

export module input.raw_event;

export import input.raw;

export namespace input {
    enum class RawInputEventType : std::uint8_t {
        None,
        Button,
        Pointer,
        Axis,
        Encoder,
    };

    enum class PointerAction : std::uint8_t {
        Down,
        Move,
        Up,
    };

    struct RawInputEvent {
        RawInputEventType type{RawInputEventType::None};
        std::uint32_t     ms{0};

        Button button{Button::Up};
        bool   pressed{false};

        PointerRaw   pointer{};
        PointerAction pointer_action{PointerAction::Move};

        AxisRaw      axis{};
        std::int16_t encoder_delta{0};
    };
} // namespace input

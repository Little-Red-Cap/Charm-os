#pragma once

#include <concepts>
#include <cstdint>

namespace charm::cap {

enum class InputButton : std::uint8_t {
    up = 0,
    down,
    enter,
    back,
};

enum class PointerAction : std::uint8_t {
    down = 0,
    move,
    up,
    cancel,
};

enum class ButtonEdge : std::uint8_t {
    none = 0,
    pressed,
    released,
};

struct EncoderSample {
    std::int16_t detent_delta{0};
    bool pressed{false};
};

struct PointerSample {
    bool detected{false};
    bool down{false};
    std::uint16_t x{0};
    std::uint16_t y{0};
    std::uint16_t max_x{0};
    std::uint16_t max_y{0};
    std::uint8_t id{0};
    std::uint8_t contacts{0};
};

struct InputFrame {
    EncoderSample encoder1{};
    EncoderSample encoder2{};
    PointerSample pointer{};
};

struct PointerEvent {
    PointerAction action{PointerAction::move};
    PointerSample sample{};
};

struct InputObservation {
    bool has_pointer{false};
    PointerEvent pointer{};
    ButtonEdge encoder1_button{ButtonEdge::none};
    ButtonEdge encoder2_button{ButtonEdge::none};
};

class InputFrameTracker {
public:
    void reset_pointer() noexcept {
        pointer_initialized_ = false;
        pointer_down_ = false;
        last_pointer_ = {};
    }

    [[nodiscard]] InputObservation observe(const InputFrame& frame) noexcept {
        InputObservation observation{};
        observation.encoder1_button = observe_button_edge(encoder1_pressed_, frame.encoder1.pressed);
        observation.encoder2_button = observe_button_edge(encoder2_pressed_, frame.encoder2.pressed);

        const auto& pointer = frame.pointer;
        if (!pointer_initialized_) {
            pointer_initialized_ = true;
            if (pointer.detected && pointer.down) {
                observation.has_pointer = true;
                observation.pointer.action = PointerAction::down;
                observation.pointer.sample = pointer;
            }
            last_pointer_ = pointer;
            pointer_down_ = pointer.down;
            return observation;
        }

        if (pointer_down_ && !pointer.detected) {
            auto sample = last_pointer_;
            sample.down = false;
            observation.has_pointer = true;
            observation.pointer.action = PointerAction::cancel;
            observation.pointer.sample = sample;
        } else if (pointer.down && !pointer_down_) {
            observation.has_pointer = true;
            observation.pointer.action = PointerAction::down;
            observation.pointer.sample = pointer;
        } else if (!pointer.down && pointer_down_) {
            observation.has_pointer = true;
            observation.pointer.action = PointerAction::up;
            observation.pointer.sample = pointer;
        } else if (pointer.down) {
            observation.has_pointer = true;
            observation.pointer.action = PointerAction::move;
            observation.pointer.sample = pointer;
        }

        last_pointer_ = pointer;
        pointer_down_ = pointer.down;
        return observation;
    }

private:
    [[nodiscard]] static ButtonEdge observe_button_edge(bool& last_down, const bool current_down) noexcept {
        if (current_down == last_down) {
            return ButtonEdge::none;
        }

        last_down = current_down;
        return current_down ? ButtonEdge::pressed : ButtonEdge::released;
    }

    bool pointer_initialized_{false};
    bool pointer_down_{false};
    bool encoder1_pressed_{false};
    bool encoder2_pressed_{false};
    PointerSample last_pointer_{};
};

template <class T>
concept InputSource = requires(T& input) {
    { input.sample() } -> std::same_as<InputFrame>;
};

} // namespace charm::cap

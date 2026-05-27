#pragma once

#include <cstdint>
#include <optional>

#include "capabilities/input.hpp"
#include "input.h"

namespace h747::input {

struct State {
    input_state_t raw{};

    [[nodiscard]] bool initialized() const noexcept {
        return raw.initialized != 0U;
    }

    [[nodiscard]] bool touch_ready() const noexcept {
        return raw.touch.ready != 0U;
    }

    [[nodiscard]] bool touch_detected() const noexcept {
        return raw.touch.detected != 0U;
    }

    [[nodiscard]] bool touch_down() const noexcept {
        return raw.touch.down != 0U;
    }

    [[nodiscard]] std::uint8_t touch_id() const noexcept {
        return raw.touch.last_id;
    }

    [[nodiscard]] std::uint16_t touch_x() const noexcept {
        return raw.touch.x;
    }

    [[nodiscard]] std::uint16_t touch_y() const noexcept {
        return raw.touch.y;
    }

    [[nodiscard]] std::uint16_t touch_max_x() const noexcept {
        return raw.touch.max_x;
    }

    [[nodiscard]] std::uint16_t touch_max_y() const noexcept {
        return raw.touch.max_y;
    }

    [[nodiscard]] std::uint8_t touch_contacts() const noexcept {
        return raw.touch.contacts;
    }

    [[nodiscard]] std::int16_t encoder1_detent_delta() const noexcept {
        return raw.encoder1.detent_delta;
    }

    [[nodiscard]] std::int16_t encoder2_detent_delta() const noexcept {
        return raw.encoder2.detent_delta;
    }

    [[nodiscard]] bool encoder1_pressed() const noexcept {
        return raw.encoder1.button_pressed != 0U;
    }

    [[nodiscard]] bool encoder2_pressed() const noexcept {
        return raw.encoder2.button_pressed != 0U;
    }

    [[nodiscard]] charm::cap::InputFrame frame() const noexcept {
        return charm::cap::InputFrame{
            .encoder1 = charm::cap::EncoderSample{
                .detent_delta = static_cast<std::int16_t>(raw.encoder1.detent_delta),
                .pressed = raw.encoder1.button_pressed != 0U,
            },
            .encoder2 = charm::cap::EncoderSample{
                .detent_delta = static_cast<std::int16_t>(raw.encoder2.detent_delta),
                .pressed = raw.encoder2.button_pressed != 0U,
            },
            .pointer = charm::cap::PointerSample{
                .detected = raw.touch.detected != 0U,
                .down = raw.touch.down != 0U,
                .x = raw.touch.x,
                .y = raw.touch.y,
                .max_x = raw.touch.max_x,
                .max_y = raw.touch.max_y,
                .id = raw.touch.last_id,
                .contacts = raw.touch.contacts,
            },
        };
    }
};

class Service {
public:
    void init() const noexcept {
        input_init();
    }

    void poll() const noexcept {
        input_poll();
    }

    [[nodiscard]] bool probe_touch() const noexcept {
        return input_touch_probe() != 0U;
    }

    [[nodiscard]] State state() const noexcept {
        return State{input_state()};
    }

    [[nodiscard]] State snapshot() const noexcept {
        return State{input_snapshot()};
    }

    [[nodiscard]] charm::cap::InputFrame sample() const noexcept {
        return state().frame();
    }
};

} // namespace h747::input

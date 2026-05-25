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
        const auto s = input_state();
        return charm::cap::InputFrame{
            .encoder1 = charm::cap::EncoderSample{
                .detent_delta = static_cast<std::int16_t>(s.encoder1.detent_delta),
                .pressed = s.encoder1.button_pressed != 0U,
            },
            .encoder2 = charm::cap::EncoderSample{
                .detent_delta = static_cast<std::int16_t>(s.encoder2.detent_delta),
                .pressed = s.encoder2.button_pressed != 0U,
            },
            .pointer = charm::cap::PointerSample{
                .detected = s.touch.detected != 0U,
                .down = s.touch.down != 0U,
                .x = s.touch.x,
                .y = s.touch.y,
                .max_x = s.touch.max_x,
                .max_y = s.touch.max_y,
                .id = s.touch.last_id,
                .contacts = s.touch.contacts,
            },
        };
    }
};

} // namespace h747::input

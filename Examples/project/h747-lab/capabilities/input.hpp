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

template <class T>
concept InputSource = requires(T& input) {
    { input.sample() } -> std::same_as<InputFrame>;
};

} // namespace charm::cap

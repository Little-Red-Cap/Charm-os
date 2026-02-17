//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>

export module input.events;

export namespace input {

    enum class Type : std::uint8_t { Key };

    enum class Key : std::uint8_t {
        Up,
        Down,
        Enter,
        Back,
        };

    struct Event {
        Type type{Type::Key};
        Key  key{Key::Up};
        bool pressed{false};      // true = down, false = up
        std::uint32_t ms{0};      // 时间戳（由后端填）
    };

} // namespace input

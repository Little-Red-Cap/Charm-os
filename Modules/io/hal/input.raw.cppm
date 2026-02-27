//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>
#include <concepts>
#include <optional>

export module input.raw;

export namespace input
{
    // Raw layer describes facts only, no semantics.
    // Any hardware (GPIO/matrix/ADC/UART/SDL) should map to these.

    enum class Button : std::uint8_t {
        Up,
        Down,
        Enter,
        Back,
        // Reserved: Shift/Fn/RotaryPress can be added later.
    };

    struct PointerRaw {
        bool         down{false};
        std::int16_t x{0};
        std::int16_t y{0};
        std::uint8_t id{0}; // Pointer id (multi-touch).
    };

    struct AxisRaw {
        // Recommended normalized range: [-32767, 32767].
        std::int16_t x{0};
        std::int16_t y{0};
    };

    // RawSource contract (concept-only, zero-cost):
    // - is_down(Button) -> bool
    // - read_pointer() -> PointerRaw (down=false if no touch).
    // - read_axis() -> AxisRaw       (0 if no stick).

    // RawSource concept: optional encoder support via pop_encoder_ab().
    template <class T>
    concept RawSource = requires(T t, const T ct, Button b)
    {
        { ct.is_down(b) } -> std::same_as<bool>;
        { ct.read_pointer() } -> std::same_as<PointerRaw>;
        { ct.read_axis() } -> std::same_as<AxisRaw>;
        { t.pop_encoder_ab() } -> std::same_as<std::optional<std::uint8_t>>;
    };
} // namespace input

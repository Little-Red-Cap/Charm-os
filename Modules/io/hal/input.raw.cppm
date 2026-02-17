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
    // Raw 层只描述“事实”，不做语义。
    // 任何硬件：GPIO/矩阵/ADC/串口注入/SDL，都应该能表达为这些。

    enum class Button : std::uint8_t {
        Up,
        Down,
        Enter,
        Back,
        // 预留：Shift/Fn/RotaryPress 等可再加
    };

    struct PointerRaw {
        bool         down{false};
        std::int16_t x{0};
        std::int16_t y{0};
        std::uint8_t id{0}; // 多指可用
    };

    struct AxisRaw {
        // 归一化范围建议：[-32767, 32767]
        std::int16_t x{0};
        std::int16_t y{0};
    };

    // 一个 RawSource 的契约（用模板/概念做零成本）：
    // - is_down(Button) -> bool
    // - read_pointer() -> PointerRaw （没有触摸就返回 down=false）
    // - read_axis() -> AxisRaw       （没有摇杆就返回 0）

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

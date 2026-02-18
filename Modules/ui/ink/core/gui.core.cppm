//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>
export module gui.core;

export import ui.common;

export namespace gui {
    using Point = ui::PointT<std::int16_t>;
    using Size = ui::SizeT<std::int16_t>;
    using Rect = ui::RectT<std::int16_t>;

    [[nodiscard]] inline std::uint32_t hash_text(const char* s) noexcept {
        if (!s) return 0;
        std::uint32_t h = 2166136261u;
        while (*s) {
            h ^= static_cast<std::uint8_t>(*s++);
            h *= 16777619u;
        }
        return h;
    }

    [[nodiscard]] constexpr bool contains(const Rect& r, std::int16_t px, std::int16_t py) noexcept {
        return ui::contains(r, px, py);
    }
}

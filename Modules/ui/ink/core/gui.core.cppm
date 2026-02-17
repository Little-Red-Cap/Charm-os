//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>
export module gui.core;


export namespace gui {
    struct Point { std::int16_t x{}, y{}; };
    struct Size  { std::int16_t w{}, h{}; };
    struct Rect  { std::int16_t x{}, y{}, w{}, h{}; };

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
        return (px >= r.x) && (py >= r.y) && (px < r.x + r.w) && (py < r.y + r.h);
    }
}

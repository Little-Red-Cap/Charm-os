module;
#include <cstdint>
export module charm.core.geometry;

export
struct Point {
    int32_t x = 0;
    int32_t y = 0;
};

export
struct Size {
    int32_t w = 0;
    int32_t h = 0;
};

export
struct Rect {
    int32_t x = 0;
    int32_t y = 0;
    int32_t w = 0;
    int32_t h = 0;

    constexpr bool contains(int32_t px, int32_t py) const noexcept {
        return px >= x && py >= y && px < x + w && py < y + h;
    }
};

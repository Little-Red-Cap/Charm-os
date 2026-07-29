module;
#include <cstdint>
#include <tuple>
#include <algorithm>
export module charm.gfx.color;

export
struct rgb {
    uint8_t r, g, b;

    constexpr rgb(const uint8_t rr = 0, const uint8_t gg = 0, const uint8_t bb = 0) noexcept
        : r(rr), g(gg), b(bb) {}

    constexpr bool operator==(rgb other) const noexcept {
        return r==other.r && g==other.g && b==other.b;
    }
};

export
struct alignas(4) rgba {
    uint8_t r, g, b, a;

    constexpr rgba(const uint8_t rr = 0, const uint8_t gg = 0, const uint8_t bb = 0, const uint8_t aa = 255) noexcept
        : r(rr), g(gg), b(bb), a(aa) {}

    [[nodiscard]] constexpr rgba blend_over(const rgba& bg) const noexcept {
        // Simple alpha blend: out = src + bg*(1 - a).
        float alpha = a / 255.0f;
        auto blend_comp = [&](uint8_t src_c, uint8_t bg_c){
            return uint8_t(src_c * alpha + bg_c * (1 - alpha));
        };
        return {
            blend_comp(r, bg.r),
            blend_comp(g, bg.g),
            blend_comp(b, bg.b),
            uint8_t( (alpha + bg.a/255.0f*(1-alpha)) * 255 )
        };
    }
};

static_assert(sizeof(rgba) == 4);
static_assert(alignof(rgba) == 4);
static_assert(noexcept(rgb{}));
static_assert(noexcept(rgba{}));

// Color space conversion example (RGB -> HSV).
export
constexpr std::tuple<float, float, float> rgb_to_hsv(const rgb& c) noexcept {
    float r = c.r / 255.0f;
    float g = c.g / 255.0f;
    float b = c.b / 255.0f;
    float mx = std::max({r,g,b}), mn = std::min({r,g,b});
    float d = mx - mn;
    float h = 0.f, s = mx==0 ? 0.f : d / mx, v = mx;
    if (d > 0.f) {
        if (mx == r)       h = 60 * ( (g-b)/d + (g < b ? 6 : 0) );
        else if (mx == g)  h = 60 * ( (b-r)/d + 2 );
        else                h = 60 * ( (r-g)/d + 4 );
    }
    return {h, s, v};
}

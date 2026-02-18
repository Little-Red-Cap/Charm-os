module;
#include <cstdint>
export module ui.geometry;

export namespace ui {
    template<typename T>
    struct PointT {
        T x{};
        T y{};
    };

    template<typename T>
    struct SizeT {
        T w{};
        T h{};
    };

    template<typename T>
    struct RectT {
        T x{};
        T y{};
        T w{};
        T h{};

        constexpr bool contains(T px, T py) const noexcept {
            return px >= x && py >= y && px < x + w && py < y + h;
        }
    };

    template<typename T>
    [[nodiscard]] constexpr bool contains(const RectT<T>& r, T px, T py) noexcept {
        return r.contains(px, py);
    }
}

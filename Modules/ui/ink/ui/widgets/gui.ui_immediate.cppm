module;
#include <cstdint>
export module gui.ui_immediate;
import gui.core;

export namespace gui {

    struct LayoutV {
        Rect area{};
        std::int16_t cursorY{};
        std::int16_t gap{2};

        constexpr LayoutV(Rect a, std::int16_t g=2) noexcept : area(a), cursorY(a.y), gap(g) {}

        constexpr Rect next(std::int16_t h) noexcept {
            Rect r{area.x, cursorY, area.w, h};
            cursorY = std::int16_t(cursorY + h + gap);
            return r;
        }
    };

} // namespace gui

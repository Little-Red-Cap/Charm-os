// List layout math helper: derive row visibility from scroll only.
// NOTE: Do not add focus-follow or scroll policy here; keep that in reduce_viewport.

module;
#include <cstdint>
export module alg_list_layout;

export namespace alg::list {

    struct Layout {
        std::int16_t visible_full{0};
        std::int16_t visible_draw{0};
        std::int16_t top_index{0};
        std::int16_t row_count{0};
        std::int16_t row_offset{0};
        std::int16_t scroll_y{0};
    };

    constexpr inline Layout derive_layout(std::int16_t area_h,
                                          std::int16_t item_h,
                                          std::int16_t gap,
                                          std::int16_t item_count,
                                          std::int16_t scroll_y) noexcept
    {
        Layout out{};
        if (item_h <= 0 || item_count <= 0 || area_h <= 0) return out;

        const int stride = item_h + gap;
        out.visible_full = (std::int16_t)((area_h + gap) / stride);
        out.visible_draw = (std::int16_t)((area_h + gap + (stride - 1)) / stride);
        if (out.visible_full < 1) out.visible_full = 1;
        if (out.visible_draw < 1) out.visible_draw = 1;
        if (out.visible_full > item_count) out.visible_full = item_count;
        if (out.visible_draw > item_count) out.visible_draw = item_count;

        out.row_count = out.visible_draw;
        out.row_offset = 0;

        const int total_h = item_count * stride - gap;
        int max_scroll = total_h - area_h;
        if (max_scroll < 0) max_scroll = 0;
        int sy = scroll_y;
        if (sy < 0) sy = 0;
        if (sy > max_scroll) sy = max_scroll;

        out.scroll_y = (std::int16_t)sy;
        out.top_index = (std::int16_t)(sy / stride);
        out.row_offset = (std::int16_t)(-(sy % stride));

        return out;
    }

    consteval bool test_empty_layout() noexcept
    {
        const auto a = derive_layout(0, 10, 2, 5, 0);
        const auto b = derive_layout(20, 10, 2, 0, 0);
        return a.row_count == 0 && a.top_index == 0 && a.scroll_y == 0
            && b.row_count == 0 && b.top_index == 0 && b.scroll_y == 0;
    }

    consteval bool test_scroll_clamp() noexcept
    {
        const auto neg = derive_layout(20, 10, 0, 5, -10);
        const auto hi = derive_layout(20, 10, 0, 5, 1000);
        return neg.scroll_y == 0
            && hi.scroll_y == 30
            && hi.top_index == 3
            && hi.row_offset == 0;
    }

    static_assert(test_empty_layout());
    static_assert(test_scroll_clamp());
} // namespace alg::list

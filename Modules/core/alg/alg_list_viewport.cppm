module;
#include <cstdint>

export module alg_list_viewport;

export namespace alg::list_view {
    struct ListViewport {
        std::int16_t scroll_y{0};
        std::int16_t top_index{0};
        std::int16_t row_offset{0};
        std::int16_t row_count{0};
        std::int16_t stride{0};
        std::int16_t last_area_h{0};
        std::int16_t last_item_h{0};
        std::int16_t last_gap{0};
        std::uint32_t last_ms{0};

        inline void reset() noexcept {
            scroll_y = 0;
            top_index = 0;
            row_offset = 0;
            row_count = 0;
            stride = 0;
            last_area_h = 0;
            last_item_h = 0;
            last_gap = 0;
            last_ms = 0;
        }

        inline void ensure_visible(std::int16_t focus, std::int16_t count, std::int16_t visible_count) noexcept {
            if (count <= 0 || visible_count <= 0) {
                top_index = 0;
                return;
            }

            const int max_top = (count > visible_count) ? (count - visible_count) : 0;
            int top = top_index;
            if (top < 0) top = 0;
            if (top > max_top) top = max_top;

            if (focus < top) {
                top = focus;
            } else if (focus >= top + visible_count) {
                top = focus - visible_count + 1;
            }

            if (top < 0) top = 0;
            if (top > max_top) top = max_top;
            top_index = static_cast<std::int16_t>(top);
        }

        [[nodiscard]] inline std::int16_t to_item_index(std::int16_t row_in_view) const noexcept {
            return static_cast<std::int16_t>(top_index + row_in_view);
        }
    };

    enum class ScrollFollow : std::uint8_t {
        KeepVisible = 0,
        CenterIfJump = 1,
        None = 2,
    };

    struct ViewportPolicy {
        ScrollFollow  follow{ScrollFollow::KeepVisible};
        bool          allow_overscroll{false};
        std::uint16_t base_ms{160};
        std::uint16_t jump_ms_scale_pct{50};
        bool          accel_enabled{false};
        std::uint16_t accel_delay_ms{350};
        std::uint16_t accel_interval_ms{120};
        std::uint8_t  accel_step{1};
    };

    [[nodiscard]] inline ListViewport reduce_viewport(const ListViewport& prev,
                                                      std::int16_t area_h,
                                                      std::int16_t item_h,
                                                      std::int16_t gap,
                                                      std::int16_t count,
                                                      std::int16_t focus_index,
                                                      std::int16_t focus_dir,
                                                      bool         jump,
                                                      const ViewportPolicy& policy,
                                                      std::uint32_t now_ms) noexcept {
        (void)focus_dir;
        ListViewport next = prev;

        next.last_area_h = area_h;
        next.last_item_h = item_h;
        next.last_gap = gap;
        next.stride = static_cast<std::int16_t>(item_h + gap);

        if (item_h <= 0 || count <= 0 || area_h <= 0) {
            next.scroll_y = 0;
            next.top_index = 0;
            next.row_offset = 0;
            next.row_count = 0;
            next.last_ms = now_ms;
            return next;
        }

        const int stride = item_h + gap;
        int visible_draw = (area_h + gap + (stride - 1)) / stride;
        if (visible_draw < 1) visible_draw = 1;
        if (visible_draw > count) visible_draw = count;
        next.row_count = static_cast<std::int16_t>(visible_draw);

        if (focus_index < 0) focus_index = 0;
        if (focus_index > count - 1) focus_index = static_cast<std::int16_t>(count - 1);

        const int total_h = count * stride - gap;
        int max_scroll = total_h - area_h;
        if (max_scroll < 0) max_scroll = 0;

        int target_scroll = prev.scroll_y;
        if (!policy.allow_overscroll) {
            if (target_scroll < 0) target_scroll = 0;
            if (target_scroll > max_scroll) target_scroll = max_scroll;
        }

        if (policy.follow != ScrollFollow::None) {
            const int focus_top = focus_index * stride;
            const int focus_bottom = focus_top + item_h;
            if (policy.follow == ScrollFollow::CenterIfJump && jump) {
                target_scroll = focus_top - area_h / 2 + item_h / 2;
            } else {
                if (focus_top < target_scroll) {
                    target_scroll = focus_top;
                } else if (focus_bottom > target_scroll + area_h) {
                    target_scroll = focus_bottom - area_h;
                }
            }
            if (!policy.allow_overscroll) {
                if (target_scroll < 0) target_scroll = 0;
                if (target_scroll > max_scroll) target_scroll = max_scroll;
            }
        }

        int sy = prev.scroll_y;
        if (!policy.allow_overscroll) {
            if (sy < 0) sy = 0;
            if (sy > max_scroll) sy = max_scroll;
        }

        std::uint32_t last = prev.last_ms;
        if (last == 0) last = now_ms;
        const std::uint32_t dt = now_ms - last;
        std::uint32_t duration = policy.base_ms;
        if (jump && policy.jump_ms_scale_pct > 0) {
            duration = (duration * policy.jump_ms_scale_pct) / 100;
            if (duration == 0) duration = 1;
        }
        if (duration == 0) {
            sy = target_scroll;
        } else {
            int diff = target_scroll - sy;
            if (diff != 0) {
                int step = static_cast<int>((static_cast<std::int64_t>(diff) * static_cast<std::int64_t>(dt))
                                            / static_cast<std::int64_t>(duration));
                if (step == 0) step = (diff > 0) ? 1 : -1;
                if ((diff > 0 && step > diff) || (diff < 0 && step < diff)) step = diff;
                sy += step;
            }
        }
        next.last_ms = now_ms;

        if (!policy.allow_overscroll) {
            if (sy < 0) sy = 0;
            if (sy > max_scroll) sy = max_scroll;
        }

        next.scroll_y = static_cast<std::int16_t>(sy);
        next.top_index = static_cast<std::int16_t>(sy / stride);
        next.row_offset = static_cast<std::int16_t>(-(sy % stride));

        return next;
    }
} // namespace alg::list_view

module;

export module player.time_utils;

export namespace player::time_utils {
    struct WeekStamp {
        int key{0};
        int day_index{0};
    };

    struct WeekStampSource {
        using ReadFn = bool (*)(void* ctx, WeekStamp& out) noexcept;

        void* ctx{nullptr};
        ReadFn read_fn{nullptr};
    };

    namespace detail {
        inline WeekStampSource source{};
    }

    inline void set_week_stamp_source(WeekStampSource source) noexcept {
        detail::source = source;
    }

    inline void reset_week_stamp_source() noexcept {
        detail::source = {};
    }

    inline WeekStamp current_week_stamp() noexcept {
        WeekStamp out{};
        const auto& source = detail::source;
        return source.read_fn && source.read_fn(source.ctx, out) ? out : WeekStamp{};
    }
}

module;
#include <ctime>

export module player.time_utils;

namespace player::time_utils::detail {
    inline bool local_time_from_epoch(std::time_t epoch, std::tm& out) noexcept {
#if defined(_WIN32)
        return ::localtime_s(&out, &epoch) == 0;
#elif defined(__unix__) || defined(__APPLE__)
        return ::localtime_r(&epoch, &out) != nullptr;
#else
        const std::tm* local = std::localtime(&epoch);
        if (!local) return false;
        out = *local;
        return true;
#endif
    }
}

export namespace player::time_utils {
    struct WeekStamp {
        int key{0};
        int day_index{0};
    };

    inline WeekStamp current_week_stamp() noexcept {
        const std::time_t now_epoch = std::time(nullptr);
        if (now_epoch == static_cast<std::time_t>(-1)) return {};

        std::tm local{};
        if (!detail::local_time_from_epoch(now_epoch, local)) return {};

        const int monday_index = (local.tm_wday + 6) % 7;
        const std::time_t monday_epoch =
            now_epoch - static_cast<std::time_t>(monday_index) * 24 * 60 * 60;

        std::tm monday{};
        if (!detail::local_time_from_epoch(monday_epoch, monday)) {
            return WeekStamp{0, monday_index};
        }

        const int key = (monday.tm_year + 1900) * 10000
            + (monday.tm_mon + 1) * 100
            + monday.tm_mday;
        return WeekStamp{key, monday_index};
    }
}

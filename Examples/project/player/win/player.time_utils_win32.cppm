module;

#include <ctime>

export module player.time_utils_win32;

import player.time_utils;

export namespace player::time_utils_win32 {
    inline bool read_week_stamp(void*, player::time_utils::WeekStamp& out) noexcept {
        const std::time_t now_epoch = std::time(nullptr);
        if (now_epoch == static_cast<std::time_t>(-1)) {
            return false;
        }

        std::tm local{};
        if (::localtime_s(&local, &now_epoch) != 0) {
            return false;
        }

        const int monday_index = (local.tm_wday + 6) % 7;
        const std::time_t monday_epoch =
            now_epoch - static_cast<std::time_t>(monday_index) * 24 * 60 * 60;
        std::tm monday{};
        if (::localtime_s(&monday, &monday_epoch) != 0) {
            return false;
        }

        out.key = (monday.tm_year + 1900) * 10000
            + (monday.tm_mon + 1) * 100
            + monday.tm_mday;
        out.day_index = monday_index;
        return true;
    }

    inline void bind() noexcept {
        player::time_utils::set_week_stamp_source({nullptr, &read_week_stamp});
    }
}

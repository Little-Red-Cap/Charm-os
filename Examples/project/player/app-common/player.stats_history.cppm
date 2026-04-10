module;

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

export module player.stats_history;

import service.fixed_vector;
import fs_core;
import fs_errno;
import fs_vfs;
import util.core;

export namespace player {
    struct ListeningStatsWeekRecord {
        int week_key{0};
        std::array<int, 7> seconds{};
        int total_plays{0};
    };

    using ListeningStatsWeekList = service::FixedVector<ListeningStatsWeekRecord, 12>;

    struct ListeningStatsHistory {
        ListeningStatsWeekList weeks{};
    };

    inline constexpr std::string_view kListeningStatsHistoryPath{"/player_weekly_stats.txt"};

    namespace detail {
        constexpr std::size_t kHistoryIoBytes = 2048;

#if defined(_WIN32)
        inline int scan_history_line(const char* line,
                                     int* week_key,
                                     int* total_plays,
                                     int* s0,
                                     int* s1,
                                     int* s2,
                                     int* s3,
                                     int* s4,
                                     int* s5,
                                     int* s6) noexcept {
            return ::sscanf_s(line,
                              "week %d plays %d seconds %d %d %d %d %d %d %d",
                              week_key, total_plays, s0, s1, s2, s3, s4, s5, s6);
        }
#else
        inline int scan_history_line(const char* line,
                                     int* week_key,
                                     int* total_plays,
                                     int* s0,
                                     int* s1,
                                     int* s2,
                                     int* s3,
                                     int* s4,
                                     int* s5,
                                     int* s6) noexcept {
            return std::sscanf(line,
                               "week %d plays %d seconds %d %d %d %d %d %d %d",
                               week_key, total_plays, s0, s1, s2, s3, s4, s5, s6);
        }
#endif

        inline fs::OpenFlags combine_open_flags(fs::OpenFlags a, fs::OpenFlags b) noexcept {
            const auto av = static_cast<util::u32>(a);
            const auto bv = static_cast<util::u32>(b);
            return static_cast<fs::OpenFlags>(av | bv);
        }

        inline void sort_history_desc(ListeningStatsHistory& history) noexcept {
            const auto n = history.weeks.size();
            if (n < 2) return;
            for (std::size_t i = 0; i + 1 < n; ++i) {
                for (std::size_t j = 0; j + 1 < n - i; ++j) {
                    if (history.weeks[j].week_key < history.weeks[j + 1].week_key) {
                        const auto tmp = history.weeks[j];
                        history.weeks[j] = history.weeks[j + 1];
                        history.weeks[j + 1] = tmp;
                    }
                }
            }
        }
    }

    inline ListeningStatsWeekRecord* find_listening_stats_week(ListeningStatsHistory& history,
                                                               int week_key) noexcept {
        for (std::size_t i = 0; i < history.weeks.size(); ++i) {
            if (history.weeks[i].week_key == week_key) {
                return &history.weeks[i];
            }
        }
        return nullptr;
    }

    inline const ListeningStatsWeekRecord* find_listening_stats_week(const ListeningStatsHistory& history,
                                                                     int week_key) noexcept {
        for (std::size_t i = 0; i < history.weeks.size(); ++i) {
            if (history.weeks[i].week_key == week_key) {
                return &history.weeks[i];
            }
        }
        return nullptr;
    }

    inline void upsert_listening_stats_week(ListeningStatsHistory& history,
                                            const ListeningStatsWeekRecord& value) noexcept {
        if (auto* existing = find_listening_stats_week(history, value.week_key)) {
            *existing = value;
            detail::sort_history_desc(history);
            return;
        }

        if (!history.weeks.push_back(value)) {
            std::size_t oldest = 0;
            for (std::size_t i = 1; i < history.weeks.size(); ++i) {
                if (history.weeks[i].week_key < history.weeks[oldest].week_key) {
                    oldest = i;
                }
            }
            if (history.weeks[oldest].week_key < value.week_key) {
                history.weeks[oldest] = value;
            }
        }
        detail::sort_history_desc(history);
    }

    inline ListeningStatsHistory load_listening_stats_history(std::string_view path = kListeningStatsHistoryPath) noexcept {
        ListeningStatsHistory history{};
        fs::File f{};
        const auto open_st = fs::vfs_open(path, f);
        if (!open_st) {
            return history;
        }

        std::array<util::u8, detail::kHistoryIoBytes> bytes{};
        const std::size_t want = std::min<std::size_t>(static_cast<std::size_t>(std::max<util::i64>(f.node.size, 0)), bytes.size() - 1);
        if (want != 0) {
            auto read_st = fs::vfs_read(f, std::span<util::u8>{bytes.data(), want});
            (void)fs::vfs_close(f);
            if (!read_st) {
                return ListeningStatsHistory{};
            }
        } else {
            (void)fs::vfs_close(f);
        }
        bytes[want] = 0;

        const char* text = reinterpret_cast<const char*>(bytes.data());
        std::size_t cursor = 0;
        while (cursor < want) {
            std::size_t line_end = cursor;
            while (line_end < want && text[line_end] != '\n') ++line_end;
            std::size_t line_len = line_end - cursor;
            while (line_len != 0 && (text[cursor + line_len - 1] == '\r' || text[cursor + line_len - 1] == '\n')) {
                --line_len;
            }
            if (line_len != 0) {
                std::array<char, 192> line_buf{};
                const std::size_t copy_len = std::min<std::size_t>(line_len, line_buf.size() - 1);
                std::memcpy(line_buf.data(), text + cursor, copy_len);
                line_buf[copy_len] = 0;

                ListeningStatsWeekRecord rec{};
                const int matched = detail::scan_history_line(line_buf.data(),
                                                              &rec.week_key,
                                                              &rec.total_plays,
                                                              &rec.seconds[0],
                                                              &rec.seconds[1],
                                                              &rec.seconds[2],
                                                              &rec.seconds[3],
                                                              &rec.seconds[4],
                                                              &rec.seconds[5],
                                                              &rec.seconds[6]);
                if (matched == 9 && rec.week_key > 0) {
                    upsert_listening_stats_week(history, rec);
                }
            }
            cursor = (line_end < want) ? (line_end + 1) : want;
        }

        return history;
    }

    inline bool save_listening_stats_history(const ListeningStatsHistory& history,
                                             std::string_view path = kListeningStatsHistoryPath) noexcept {
        std::array<char, detail::kHistoryIoBytes> out{};
        int used = std::snprintf(out.data(), out.size(), "v1\n");
        if (used < 0 || static_cast<std::size_t>(used) >= out.size()) {
            return false;
        }

        for (std::size_t i = 0; i < history.weeks.size(); ++i) {
            const auto& rec = history.weeks[i];
            const int wrote = std::snprintf(out.data() + used,
                                            out.size() - static_cast<std::size_t>(used),
                                            "week %d plays %d seconds %d %d %d %d %d %d %d\n",
                                            rec.week_key,
                                            rec.total_plays,
                                            rec.seconds[0],
                                            rec.seconds[1],
                                            rec.seconds[2],
                                            rec.seconds[3],
                                            rec.seconds[4],
                                            rec.seconds[5],
                                            rec.seconds[6]);
            if (wrote < 0) return false;
            used += wrote;
            if (static_cast<std::size_t>(used) >= out.size()) {
                return false;
            }
        }

        fs::File f{};
        auto flags = detail::combine_open_flags(fs::OpenFlags::write, fs::OpenFlags::create);
        flags = detail::combine_open_flags(flags, fs::OpenFlags::trunc);
        const auto open_st = fs::vfs_open(path, f, flags);
        if (!open_st) {
            return false;
        }
        const auto write_st = fs::vfs_write(f,
            std::span<const util::u8>{reinterpret_cast<const util::u8*>(out.data()), static_cast<std::size_t>(used)});
        (void)fs::vfs_close(f);
        return static_cast<bool>(write_st);
    }
}

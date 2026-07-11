module;

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

#ifndef CHARM_PLAYER_RECENT_HISTORY_VFS
#define CHARM_PLAYER_RECENT_HISTORY_VFS 1
#endif

export module player.recent_history;

#if CHARM_PLAYER_RECENT_HISTORY_VFS
import fs_core;
import fs_vfs;
#endif
import player.fixed_string;
import player.product_config;
import service.fixed_vector;
#if CHARM_PLAYER_RECENT_HISTORY_VFS
import util.core;
#endif

export namespace player {
    struct RecentTrackRecord {
        FixedString<product_config::path_text_capacity> path{};
        std::uint32_t play_count{0};
        std::uint64_t last_played_ms{0};
    };

    using RecentTrackList =
        service::FixedVector<RecentTrackRecord,
                             product_config::recent_track_history_entries>;

    struct RecentTrackHistory {
        RecentTrackList tracks{};
    };

    inline constexpr std::string_view recent_track_history_path() noexcept {
        return "/player_recent_tracks.txt";
    }

    namespace recent_detail {
        constexpr std::size_t kHistoryIoBytes =
            product_config::recent_track_history_io_bytes;

#if CHARM_PLAYER_RECENT_HISTORY_VFS
        inline fs::OpenFlags combine_open_flags(fs::OpenFlags a,
                                                fs::OpenFlags b) noexcept {
            const auto av = static_cast<util::u32>(a);
            const auto bv = static_cast<util::u32>(b);
            return static_cast<fs::OpenFlags>(av | bv);
        }
#endif

        inline bool match_token(const char*& p, const char* token) noexcept {
            while (*p == ' ' || *p == '\t') ++p;
            const char* t = token;
            while (*t != 0) {
                if (*p != *t) return false;
                ++p;
                ++t;
            }
            return true;
        }

        inline bool scan_u64(const char*& p, std::uint64_t& out) noexcept {
            while (*p == ' ' || *p == '\t') ++p;
            if (*p < '0' || *p > '9') return false;
            std::uint64_t value = 0;
            while (*p >= '0' && *p <= '9') {
                value = value * 10u + static_cast<std::uint64_t>(*p - '0');
                ++p;
            }
            out = value;
            return true;
        }

        inline bool scan_u32(const char*& p, std::uint32_t& out) noexcept {
            std::uint64_t value = 0;
            if (!scan_u64(p, value)) return false;
            if (value > 0xFFFFFFFFull) return false;
            out = static_cast<std::uint32_t>(value);
            return true;
        }

        inline std::string_view trim_space(std::string_view value) noexcept {
            while (!value.empty()
                   && (value.front() == ' ' || value.front() == '\t')) {
                value.remove_prefix(1);
            }
            while (!value.empty()
                   && (value.back() == ' '
                       || value.back() == '\t'
                       || value.back() == '\r'
                       || value.back() == '\n')) {
                value.remove_suffix(1);
            }
            return value;
        }

        inline bool scan_history_line(const char* line,
                                      RecentTrackRecord& rec) noexcept {
            if (!line) return false;
            const char* p = line;
            std::uint64_t last_played_ms = 0;
            std::uint32_t play_count = 0;
            if (!match_token(p, "last")
                || !scan_u64(p, last_played_ms)
                || !match_token(p, "plays")
                || !scan_u32(p, play_count)
                || !match_token(p, "path")) {
                return false;
            }

            const auto path = trim_space(std::string_view{p});
            if (path.empty()) return false;
            rec.path.assign(path);
            rec.last_played_ms = last_played_ms;
            rec.play_count = play_count;
            return !rec.path.empty();
        }

        inline void sort_history_desc(RecentTrackHistory& history) noexcept {
            const auto n = history.tracks.size();
            if (n < 2) return;
            for (std::size_t i = 0; i + 1 < n; ++i) {
                for (std::size_t j = 0; j + 1 < n - i; ++j) {
                    const auto& a = history.tracks[j];
                    const auto& b = history.tracks[j + 1];
                    if (a.last_played_ms < b.last_played_ms) {
                        const auto tmp = history.tracks[j];
                        history.tracks[j] = history.tracks[j + 1];
                        history.tracks[j + 1] = tmp;
                    }
                }
            }
        }
    }

    inline RecentTrackRecord* find_recent_track(RecentTrackHistory& history,
                                                std::string_view path) noexcept {
        if (path.empty()) return nullptr;
        for (std::size_t i = 0; i < history.tracks.size(); ++i) {
            if (history.tracks[i].path.view() == path) {
                return &history.tracks[i];
            }
        }
        return nullptr;
    }

    inline const RecentTrackRecord* find_recent_track(const RecentTrackHistory& history,
                                                      std::string_view path) noexcept {
        if (path.empty()) return nullptr;
        for (std::size_t i = 0; i < history.tracks.size(); ++i) {
            if (history.tracks[i].path.view() == path) {
                return &history.tracks[i];
            }
        }
        return nullptr;
    }

    inline void upsert_recent_track(RecentTrackHistory& history,
                                    std::string_view path,
                                    std::uint64_t played_ms) noexcept {
        if (path.empty()) return;
        if (auto* existing = find_recent_track(history, path)) {
            existing->last_played_ms = played_ms;
            if (existing->play_count != 0xFFFFFFFFu) {
                ++existing->play_count;
            }
            recent_detail::sort_history_desc(history);
            return;
        }

        RecentTrackRecord rec{};
        rec.path.assign(path);
        if (rec.path.empty()) return;
        rec.last_played_ms = played_ms;
        rec.play_count = 1;

        if (!history.tracks.push_back(rec)) {
            std::size_t oldest = 0;
            for (std::size_t i = 1; i < history.tracks.size(); ++i) {
                if (history.tracks[i].last_played_ms
                    < history.tracks[oldest].last_played_ms) {
                    oldest = i;
                }
            }
            if (history.tracks[oldest].last_played_ms <= played_ms) {
                history.tracks[oldest] = rec;
            }
        }
        recent_detail::sort_history_desc(history);
    }

    inline bool parse_recent_track_history(std::string_view text,
                                           RecentTrackHistory& history) noexcept {
        history = {};
        std::size_t cursor = 0;
        while (cursor < text.size()) {
            std::size_t line_end = cursor;
            while (line_end < text.size() && text[line_end] != '\n') ++line_end;
            std::size_t line_len = line_end - cursor;
            while (line_len != 0
                   && (text[cursor + line_len - 1] == '\r'
                       || text[cursor + line_len - 1] == '\n')) {
                --line_len;
            }
            if (line_len != 0) {
                std::array<char, product_config::recent_track_history_line_bytes> line_buf{};
                const std::size_t copy_len =
                    std::min<std::size_t>(line_len, line_buf.size() - 1);
                std::memcpy(line_buf.data(), text.data() + cursor, copy_len);
                line_buf[copy_len] = 0;

                RecentTrackRecord rec{};
                if (recent_detail::scan_history_line(line_buf.data(), rec)) {
                    if (auto* existing = find_recent_track(history, rec.path.view())) {
                        if (existing->last_played_ms <= rec.last_played_ms) {
                            *existing = rec;
                        }
                    } else {
                        (void)history.tracks.push_back(rec);
                    }
                }
            }
            cursor = (line_end < text.size()) ? (line_end + 1) : text.size();
        }
        recent_detail::sort_history_desc(history);
        return true;
    }

    inline RecentTrackHistory parse_recent_track_history(std::string_view text) noexcept {
        RecentTrackHistory history{};
        (void)parse_recent_track_history(text, history);
        return history;
    }

    inline bool format_recent_track_history(const RecentTrackHistory& history,
                                            char* out,
                                            std::size_t out_size,
                                            std::size_t& out_used) noexcept {
        out_used = 0;
        if (!out || out_size == 0) return false;
        int used = std::snprintf(out, out_size, "v1\n");
        if (used < 0 || static_cast<std::size_t>(used) >= out_size) {
            return false;
        }

        for (std::size_t i = 0; i < history.tracks.size(); ++i) {
            const auto& rec = history.tracks[i];
            if (rec.path.empty()) continue;
            const int wrote = std::snprintf(
                out + used,
                out_size - static_cast<std::size_t>(used),
                "last %llu plays %lu path %s\n",
                static_cast<unsigned long long>(rec.last_played_ms),
                static_cast<unsigned long>(rec.play_count),
                rec.path.c_str());
            if (wrote < 0) return false;
            used += wrote;
            if (static_cast<std::size_t>(used) >= out_size) {
                return false;
            }
        }

        out_used = static_cast<std::size_t>(used);
        return true;
    }

#if CHARM_PLAYER_RECENT_HISTORY_VFS
    inline RecentTrackHistory load_recent_track_history(std::string_view path) noexcept {
        RecentTrackHistory history{};
        fs::File f{};
        const auto open_st = fs::vfs_open(path, f);
        if (!open_st) {
            return history;
        }

        std::array<util::u8, recent_detail::kHistoryIoBytes> bytes{};
        const std::size_t want = std::min<std::size_t>(
            static_cast<std::size_t>(std::max<util::i64>(f.node.size, 0)),
            bytes.size() - 1);
        if (want != 0) {
            auto read_st = fs::vfs_read(f, std::span<util::u8>{bytes.data(), want});
            (void)fs::vfs_close(f);
            if (!read_st) {
                return RecentTrackHistory{};
            }
        } else {
            (void)fs::vfs_close(f);
        }
        bytes[want] = 0;

        const auto text = std::string_view{
            reinterpret_cast<const char*>(bytes.data()),
            want,
        };
        (void)parse_recent_track_history(text, history);
        return history;
    }

    inline RecentTrackHistory load_recent_track_history() noexcept {
        return load_recent_track_history(recent_track_history_path());
    }

    inline bool save_recent_track_history(const RecentTrackHistory& history,
                                          std::string_view path) noexcept {
        std::array<char, recent_detail::kHistoryIoBytes> out{};
        std::size_t used = 0;
        if (!format_recent_track_history(history, out.data(), out.size(), used)) {
            return false;
        }

        fs::File f{};
        auto flags = recent_detail::combine_open_flags(fs::OpenFlags::write,
                                                       fs::OpenFlags::create);
        flags = recent_detail::combine_open_flags(flags, fs::OpenFlags::trunc);
        const auto open_st = fs::vfs_open(path, f, flags);
        if (!open_st) {
            return false;
        }
        const auto write_st = fs::vfs_write(
            f,
            std::span<const util::u8>{
                reinterpret_cast<const util::u8*>(out.data()),
                used});
        (void)fs::vfs_close(f);
        return static_cast<bool>(write_st);
    }

    inline bool save_recent_track_history(const RecentTrackHistory& history) noexcept {
        return save_recent_track_history(history, recent_track_history_path());
    }
#endif
}

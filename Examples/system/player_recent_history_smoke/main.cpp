import player.recent_history;
import player.product_config;

#include <array>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace {
    bool expect(bool condition, const char* message) {
        if (!condition) {
            std::printf("[player-recent-history-smoke] fail: %s\n", message);
            return false;
        }
        return true;
    }

    bool expect_path(const player::RecentTrackHistory& history,
                     std::size_t index,
                     std::string_view path,
                     const char* message) {
        if (!expect(index < history.tracks.size(), message)) {
            return false;
        }
        if (history.tracks[index].path.view() != path) {
            std::printf("[player-recent-history-smoke] detail index=%zu got=%s expected=%.*s\n",
                        index,
                        history.tracks[index].path.c_str(),
                        static_cast<int>(path.size()),
                        path.data());
            return expect(false, message);
        }
        return true;
    }

    bool run_smoke() {
        player::RecentTrackHistory history{};

        player::upsert_recent_track(history, "/music/A.wav", 1000);
        player::upsert_recent_track(history, "/music/B.flac", 2000);
        player::upsert_recent_track(history, "/music/C.mp3", 1500);
        if (!expect(history.tracks.size() == 3, "three recent entries")) {
            return false;
        }
        if (!expect_path(history, 0, "/music/B.flac", "newest entry first")) {
            return false;
        }
        if (!expect_path(history, 1, "/music/C.mp3", "middle entry second")) {
            return false;
        }
        if (!expect_path(history, 2, "/music/A.wav", "oldest entry third")) {
            return false;
        }

        player::upsert_recent_track(history, "/music/A.wav", 3000);
        if (!expect(history.tracks.size() == 3, "duplicate path does not grow history")) {
            return false;
        }
        if (!expect_path(history, 0, "/music/A.wav", "duplicate path becomes newest")) {
            return false;
        }
        if (!expect(history.tracks[0].play_count == 2, "duplicate path increments play count")) {
            return false;
        }

        for (std::uint64_t i = 0; i < 20; ++i) {
            char path[64]{};
            std::snprintf(path, sizeof(path), "/music/fill-%02llu.wav",
                          static_cast<unsigned long long>(i));
            player::upsert_recent_track(history, path, 4000 + i);
        }
        if (!expect(history.tracks.size() == player::product_config::recent_track_history_entries,
                    "history is capped")) {
            return false;
        }
        if (!expect_path(history, 0, "/music/fill-19.wav", "cap keeps newest item")) {
            return false;
        }
        if (!expect(player::find_recent_track(history, "/music/B.flac") == nullptr,
                    "cap evicts older items")) {
            return false;
        }

        std::array<char, player::product_config::recent_track_history_io_bytes> text{};
        std::size_t used = 0;
        if (!expect(player::format_recent_track_history(history,
                                                        text.data(),
                                                        text.size(),
                                                        used),
                    "format recent history")) {
            return false;
        }
        if (!expect(used > 3 && std::strncmp(text.data(), "v1\n", 3) == 0,
                    "format starts with version")) {
            return false;
        }

        const auto parsed = player::parse_recent_track_history(
            std::string_view{text.data(), used});
        if (!expect(parsed.tracks.size() == history.tracks.size(),
                    "parse preserves capped count")) {
            return false;
        }
        if (!expect_path(parsed, 0, history.tracks[0].path.view(),
                         "parse preserves newest path")) {
            return false;
        }
        if (!expect(parsed.tracks[0].last_played_ms == history.tracks[0].last_played_ms,
                    "parse preserves timestamp")) {
            return false;
        }

        const auto parsed_dirty = player::parse_recent_track_history(
            "v1\n"
            "last 7 plays 2 path /music/old.wav\n"
            "bad row\n"
            "last 9 plays 4 path /music/new.wav\n"
            "last 8 plays 3 path /music/old.wav\n");
        if (!expect(parsed_dirty.tracks.size() == 2,
                    "parse ignores bad rows and deduplicates")) {
            return false;
        }
        if (!expect_path(parsed_dirty, 0, "/music/new.wav", "parse sorts newest")) {
            return false;
        }
        if (!expect_path(parsed_dirty, 1, "/music/old.wav",
                         "parse keeps newest duplicate")) {
            return false;
        }
        if (!expect(parsed_dirty.tracks[1].last_played_ms == 8
                    && parsed_dirty.tracks[1].play_count == 3,
                    "parse newest duplicate payload")) {
            return false;
        }

        return true;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::printf("[player-recent-history-smoke] ok\n");
    return 0;
}

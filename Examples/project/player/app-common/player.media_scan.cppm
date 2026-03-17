module;
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

export module player.media_scan;

import player.fixed_string;
import fs_core;
import fs_errno;
import fs_stream;
import player.fs_utils;

export namespace player {
    using MountFn = fs::Status (*)(const char* path);

    struct TrackScanResult {
        bool fs_ready{false};
        bool has_tracks{false};
        FixedString<128> status{};
        FixedString<128> mount_status{};
        std::vector<std::string> tracks{};
    };

    TrackScanResult scan_tracks(MountFn mount, const char* path) {
        TrackScanResult out{};
        out.mount_status.assign("Mounting storage...");
        if (!mount) {
            out.status.assign("Mount not configured");
            out.mount_status.assign(out.status.c_str());
            return out;
        }
        const auto mount_st = mount(path);
        out.fs_ready = static_cast<bool>(mount_st);
        if (!out.fs_ready) {
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), "Mount failed (%s)", fs_utils::fs_err_text(mount_st.err));
            out.status.assign(buf);
            out.mount_status.assign("Mount failed. Unmount VHD in Windows");
            return out;
        }

        out.mount_status.assign("Mounted");
#if defined(_WIN32) && defined(CHARM_PLAYER_FS_DUMP) && CHARM_PLAYER_FS_DUMP
        std::printf("[fs] mount ok, dump tree:\n");
        (void)fs_utils::dump_fs_tree("/", 0, 4);
#endif
        fs::Status list_st{fs::Errc::ok};
        if (!fs_utils::collect_tracks_from_dir("/music", out.tracks, nullptr, list_st)) {
            std::vector<std::string> subdirs;
            out.mount_status.assign("Scanning /...");
            const bool root_has = fs_utils::collect_tracks_from_dir("/", out.tracks, &subdirs, list_st);
            if (list_st) {
                for (const auto& dir : subdirs) {
                    fs_utils::collect_tracks_from_dir(dir, out.tracks, nullptr, list_st);
                }
            }
            if (!list_st) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "List failed (%s)", fs_utils::fs_err_text(list_st.err));
                out.status.assign(buf);
                out.mount_status.assign(out.status.c_str());
                return out;
            }
            if (!root_has && out.tracks.empty()) {
                out.status.assign("No tracks found");
                out.mount_status.assign("No tracks in /music or /");
            }
        }

        if (out.tracks.empty()) {
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), "No tracks (%s)", fs_utils::fs_err_text(list_st.err));
            out.status.assign(buf);
            if (out.mount_status.view() == "Mounted") {
                out.mount_status.assign("No tracks in /music or /");
            }
        } else if (out.mount_status.view() == "Mounted") {
            out.mount_status.assign("Ready");
        }

        out.has_tracks = !out.tracks.empty();
#if defined(_WIN32) && defined(CHARM_PLAYER_FS_DUMP) && CHARM_PLAYER_FS_DUMP
        std::printf("[fs] tracks=%zu\n", out.tracks.size());
        for (const auto& path_str : out.tracks) {
            std::printf("[fs] track: ");
            for (unsigned char ch : path_str) {
                if (std::isprint(ch)) {
                    std::printf("%c", static_cast<char>(ch));
                } else {
                    std::printf("\\x%02X", static_cast<unsigned int>(ch));
                }
            }
            std::printf("\n");
        }
#endif
        return out;
    }
}

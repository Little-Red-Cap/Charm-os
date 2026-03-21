module;
#include <cctype>
#include <cstdio>
#include <cstddef>
#include <string_view>

export module player.media_scan;

import service.fixed_vector;
import player.fixed_string;
import fs_core;
import fs_errno;
import fs_stream;
import player.fs_utils;

export namespace player {
    using MountFn = fs::Status (*)(const char* path);
#ifndef CHARM_PLAYER_FS_LOG
#define CHARM_PLAYER_FS_LOG 0
#endif

#ifndef CHARM_PLAYER_MAX_TRACKS
#define CHARM_PLAYER_MAX_TRACKS 256
#endif
#ifndef CHARM_PLAYER_MAX_SCAN_DIRS
#define CHARM_PLAYER_MAX_SCAN_DIRS 64
#endif

    constexpr std::size_t kMaxTracks = CHARM_PLAYER_MAX_TRACKS;
    constexpr std::size_t kMaxScanDirs = CHARM_PLAYER_MAX_SCAN_DIRS;
    using TrackPath = FixedString<260>;
    using TrackList = service::FixedVector<TrackPath, kMaxTracks>;
    using DirList = service::FixedVector<TrackPath, kMaxScanDirs>;

    struct TrackScanResult {
        bool fs_ready{false};
        bool has_tracks{false};
        FixedString<128> status{};
        FixedString<128> mount_status{};
        TrackList tracks{};
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
#if CHARM_PLAYER_FS_LOG && defined(_WIN32) && defined(CHARM_PLAYER_FS_DUMP) && CHARM_PLAYER_FS_DUMP
        std::printf("[fs] mount ok, dump tree:\n");
        (void)fs_utils::dump_fs_tree("/", 0, 4);
#endif
        fs::Status list_st{fs::Errc::ok};
        if (!fs_utils::collect_tracks_from_dir("/music", out.tracks, nullptr, list_st)) {
            if (!list_st && list_st.err == fs::Errc::nomem) {
                out.status.assign("Track list full");
                out.mount_status.assign(out.status.c_str());
                return out;
            }
            DirList subdirs;
            out.mount_status.assign("Scanning /...");
            const bool root_has = fs_utils::collect_tracks_from_dir("/", out.tracks, &subdirs, list_st);
            if (list_st) {
                for (std::size_t i = 0; i < subdirs.size(); ++i) {
                    fs_utils::collect_tracks_from_dir(subdirs[i].view(), out.tracks, nullptr, list_st);
                    if (!list_st) break;
                }
            }
            if (!list_st) {
                if (list_st.err == fs::Errc::nomem) {
                    out.status.assign("Track list full");
                    out.mount_status.assign(out.status.c_str());
                    return out;
                }
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "List failed (%s)", fs_utils::fs_err_text(list_st.err));
                out.status.assign(buf);
                out.mount_status.assign(out.status.c_str());
                return out;
            }
            if (!root_has && out.tracks.size() == 0) {
                out.status.assign("No tracks found");
                out.mount_status.assign("No tracks in /music or /");
            }
        }

        if (out.tracks.size() == 0) {
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), "No tracks (%s)", fs_utils::fs_err_text(list_st.err));
            out.status.assign(buf);
            if (out.mount_status.view() == std::string_view("Mounted")) {
                out.mount_status.assign("No tracks in /music or /");
            }
        } else if (out.mount_status.view() == std::string_view("Mounted")) {
            out.mount_status.assign("Ready");
        }

        out.has_tracks = out.tracks.size() > 0;
#if CHARM_PLAYER_FS_LOG && defined(_WIN32) && defined(CHARM_PLAYER_FS_DUMP) && CHARM_PLAYER_FS_DUMP
        std::printf("[fs] tracks=%zu\n", out.tracks.size());
        for (std::size_t i = 0; i < out.tracks.size(); ++i) {
            const auto view = out.tracks[i].view();
            std::printf("[fs] track: ");
            for (unsigned char ch : view) {
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

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
import player.host_features;

export namespace player {
    using MountFn = fs::Status (*)(const char* path);

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
        bool track_truncated{false};
        bool dir_truncated{false};
        FixedString<128> status{};
        FixedString<128> mount_status{};
        TrackList tracks{};
    };

    namespace detail {
        bool collect_one_level_tracks(std::string_view root,
                                      TrackList& tracks,
                                      DirList& subdirs,
                                      fs::Status& out_status,
                                      bool& track_truncated,
                                      bool& dir_truncated) {
            const std::size_t before_tracks = tracks.size();
            const bool root_has = fs_utils::collect_tracks_from_dir(root, tracks, &subdirs, out_status);
            if (!out_status) {
                if (out_status.err == fs::Errc::nomem) {
                    if (tracks.size() >= kMaxTracks) {
                        track_truncated = true;
                    } else {
                        dir_truncated = true;
                    }
                }
                return tracks.size() > before_tracks;
            }
            for (std::size_t i = 0; i < subdirs.size(); ++i) {
                fs_utils::collect_tracks_from_dir(subdirs[i].view(), tracks, nullptr, out_status);
                if (!out_status) {
                    if (out_status.err == fs::Errc::nomem) {
                        track_truncated = true;
                    }
                    return tracks.size() > before_tracks;
                }
            }
            return root_has || tracks.size() > before_tracks;
        }
    }

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
        if constexpr (host_features::host_fs_dump) {
            std::printf("[fs] mount ok, dump tree:\n");
            (void)fs_utils::dump_fs_tree("/", 0, 4);
        }
        fs::Status list_st{fs::Errc::ok};
        DirList subdirs;
        const bool music_has = detail::collect_one_level_tracks("/music",
                                                                out.tracks,
                                                                subdirs,
                                                                list_st,
                                                                out.track_truncated,
                                                                out.dir_truncated);
        if (!music_has && !out.track_truncated) {
            out.mount_status.assign("Scanning /...");
            subdirs.clear();
            const bool root_has = detail::collect_one_level_tracks("/",
                                                                   out.tracks,
                                                                   subdirs,
                                                                   list_st,
                                                                   out.track_truncated,
                                                                   out.dir_truncated);
            if (!root_has && !list_st && list_st.err != fs::Errc::nomem) {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "List failed (%s)", fs_utils::fs_err_text(list_st.err));
                out.status.assign(buf);
                out.mount_status.assign(out.status.c_str());
                return out;
            }
            if (!root_has && out.tracks.size() == 0 && !out.track_truncated && !out.dir_truncated) {
                out.status.assign("No tracks found");
                out.mount_status.assign("No tracks in /music or /");
            }
        }

        if (out.tracks.size() == 0) {
            if (out.track_truncated) {
                out.status.assign("Track list full");
                out.mount_status.assign(out.status.c_str());
            } else if (out.dir_truncated) {
                out.status.assign("Scan dir list full");
                out.mount_status.assign(out.status.c_str());
            } else {
                char buf[64]{};
                std::snprintf(buf, sizeof(buf), "No tracks (%s)", fs_utils::fs_err_text(list_st.err));
                out.status.assign(buf);
                if (out.mount_status.view() == std::string_view("Mounted")) {
                    out.mount_status.assign("No tracks in /music or /");
                }
            }
        } else if (out.track_truncated) {
            out.status.assign("Track list full");
            out.mount_status.assign(out.status.c_str());
        } else if (out.dir_truncated) {
            out.status.assign("Scan dir list full");
            out.mount_status.assign(out.status.c_str());
        } else if (out.mount_status.view() == std::string_view("Mounted")) {
            out.mount_status.assign("Ready");
        }

        out.has_tracks = out.tracks.size() > 0;
        if constexpr (host_features::host_fs_dump) {
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
        }
        return out;
    }
}

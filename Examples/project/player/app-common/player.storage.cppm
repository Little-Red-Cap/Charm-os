module;
#include <cstddef>
#include <cstdio>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

export module player.storage;

import player.fixed_string;
import fs_block;
import fs_core;
import fs_errno;
import fs_fatfs;
import fs_stream;
import fs_vfs;
import player.fs_utils;
import player.media_scan;

export namespace player {
    struct StorageConfig {
        MountFn mount{nullptr};
        const char* path{nullptr};
    };

    struct StorageState {
        bool fs_ready{false};
        bool has_tracks{false};
        FixedString<128> status{};
        FixedString<128> mount_status{};
        std::vector<std::string> tracks{};
        std::vector<std::string> track_labels{};
        std::vector<std::string> track_titles{};
        std::vector<std::string> track_subtitles{};
    };

    struct StorageView {
        bool fs_ready{false};
        bool has_tracks{false};
        std::string_view status{};
        std::string_view mount_status{};
        const std::vector<std::string>* tracks{nullptr};
        const std::vector<std::string>* track_labels{nullptr};
        const std::vector<std::string>* track_titles{nullptr};
        const std::vector<std::string>* track_subtitles{nullptr};
    };

    namespace detail {
        StorageConfig g_storage_config{};
        fs::BlockDevice* g_sd_device{nullptr};

        fs::Status mount_fatfs_from_sd(const char*) {
            if (!g_sd_device) return fs::Status{fs::Errc::nosys};
            static fs::FatFsMount fat;
            auto st = fat.mount(*g_sd_device, false);
            if (!st) return st;
            fs::clear_mounts();
            (void)fs::add_mount("/", fat.mount_point());
            return fs::Status{fs::Errc::ok};
        }

        StorageConfig default_storage_config() {
#if defined(_WIN32)
            return StorageConfig{&fs_utils::mount_fatfs_from_vhd, "G:/Project/dev.vhd"};
#else
            return StorageConfig{&mount_fatfs_from_sd, nullptr};
#endif
        }
    }

    void set_storage_config(StorageConfig cfg) noexcept {
        detail::g_storage_config = cfg;
    }

    StorageConfig storage_config() noexcept {
        return detail::g_storage_config;
    }

    void set_sd_block_device(fs::BlockDevice* dev) noexcept {
        detail::g_sd_device = dev;
    }

    TrackScanResult scan_tracks_default() {
        StorageConfig cfg = detail::g_storage_config;
        if (!cfg.mount) {
            cfg = detail::default_storage_config();
        }
        return scan_tracks(cfg.mount, cfg.path);
    }

    StorageState scan_storage() {
        auto scan = scan_tracks_default();
        StorageState out{};
        out.fs_ready = scan.fs_ready;
        out.has_tracks = scan.has_tracks;
        out.status.assign(scan.status.c_str());
        out.mount_status.assign(scan.mount_status.c_str());
        out.tracks = std::move(scan.tracks);
        out.track_labels.reserve(out.tracks.size());
        out.track_titles.reserve(out.tracks.size());
        out.track_subtitles.reserve(out.tracks.size());
        for (const auto& path : out.tracks) {
            std::string_view base{path};
            const auto pos = base.find_last_of("/\\");
            if (pos != std::string_view::npos) base = base.substr(pos + 1);
            out.track_labels.emplace_back(base);
            std::string_view ext{};
            const auto dot = base.find_last_of('.');
            if (dot != std::string_view::npos && dot + 1 < base.size()) {
                ext = base.substr(dot + 1);
            }
            out.track_titles.emplace_back(base);
            if (out.track_titles.back().empty()) {
                out.track_titles.back() = "Unknown Track";
            }
            if (!ext.empty()) {
                std::string upper(ext);
                for (auto& ch : upper) {
                    ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                }
                out.track_subtitles.emplace_back(std::move(upper));
            } else {
                out.track_subtitles.emplace_back("UNKNOWN");
            }
        }
        return out;
    }

    StorageView make_storage_view(const StorageState& state) noexcept {
        StorageView out{};
        out.fs_ready = state.fs_ready;
        out.has_tracks = state.has_tracks;
        out.status = state.status.view();
        out.mount_status = state.mount_status.view();
        out.tracks = &state.tracks;
        out.track_labels = &state.track_labels;
        out.track_titles = &state.track_titles;
        out.track_subtitles = &state.track_subtitles;
        return out;
    }

    StorageConfig default_storage_config() {
        return detail::default_storage_config();
    }

    void init_storage(StorageConfig cfg) noexcept {
        set_storage_config(cfg);
    }

    void init_storage(fs::BlockDevice& dev) noexcept {
        detail::g_sd_device = &dev;
        detail::g_storage_config = StorageConfig{&detail::mount_fatfs_from_sd, nullptr};
    }

    bool check_track_ready(std::string_view vfs_path, FixedString<128>& out_status) {
        fs::File f{};
        auto st = fs::vfs_open(vfs_path, f);
        if (st) {
            (void)fs::vfs_close(f);
            out_status.assign("Ready");
            return true;
        }
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "Load failed (%s)", fs_utils::fs_err_text(st.err));
        out_status.assign(buf);
        return false;
    }
}

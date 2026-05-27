module;
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cctype>
#include <string_view>

export module player.storage;

import service.fixed_vector;
import player.fixed_string;
import fs_block;
import fs_core;
import fs_errno;
#if defined(CHARM_PLAYER_ENABLE_FATFS_STORAGE) && CHARM_PLAYER_ENABLE_FATFS_STORAGE
import fs_fatfs;
#endif
import fs_stream;
import fs_vfs;
import player.fs_utils;
import player.host_features;
import player.media_scan;
import player.product_config;

export namespace player {
    using TrackLabel = FixedString<product_config::track_label_text_capacity>;
    using TrackTitle = FixedString<product_config::track_title_text_capacity>;
    using TrackSubtitle = FixedString<product_config::track_subtitle_text_capacity>;
    using TrackLabelList = service::FixedVector<TrackLabel, kMaxTracks>;
    using TrackTitleList = service::FixedVector<TrackTitle, kMaxTracks>;
    using TrackSubtitleList = service::FixedVector<TrackSubtitle, kMaxTracks>;

    struct StorageConfig {
        MountFn mount{nullptr};
        const char* path{nullptr};
    };

    struct StorageState {
        bool fs_ready{false};
        bool has_tracks{false};
        FixedString<product_config::scan_status_text_capacity> status{};
        FixedString<product_config::scan_status_text_capacity> mount_status{};
        TrackList tracks{};
        TrackLabelList track_labels{};
        TrackTitleList track_titles{};
        TrackSubtitleList track_subtitles{};
        bool labels_ready{false};
    };

    struct StorageView {
        bool fs_ready{false};
        bool has_tracks{false};
        std::string_view status{};
        std::string_view mount_status{};
        const TrackList* tracks{nullptr};
        const TrackLabelList* track_labels{nullptr};
        const TrackTitleList* track_titles{nullptr};
        const TrackSubtitleList* track_subtitles{nullptr};
    };

    namespace detail {
        StorageConfig g_storage_config{};
        fs::BlockDevice* g_sd_device{nullptr};

#if defined(CHARM_PLAYER_ENABLE_FATFS_STORAGE) && CHARM_PLAYER_ENABLE_FATFS_STORAGE
        fs::Status mount_fatfs_from_sd(const char*) {
            if (!g_sd_device) return fs::Status{fs::Errc::nosys};
            static fs::FatFsMount fat;
            auto st = fat.mount(*g_sd_device, false);
            if (!st) return st;
            fs::clear_mounts();
            (void)fs::add_mount("/", fat.mount_point());
            return fs::Status{fs::Errc::ok};
        }
#else
        fs::Status mount_fatfs_from_sd(const char*) {
            return fs::Status{fs::Errc::nosys};
        }
#endif

        StorageConfig default_storage_config() {
            if constexpr (host_features::host_storage) {
                return StorageConfig{&fs_utils::mount_fatfs_from_vhd, product_config::host_default_vhd_path};
            }
            return StorageConfig{&mount_fatfs_from_sd, nullptr};
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

    template <typename ScanResult>
    void scan_tracks_default_into(ScanResult& out) {
        StorageConfig cfg = detail::g_storage_config;
        if (!cfg.mount) {
            cfg = detail::default_storage_config();
        }
        auto scan = scan_tracks(cfg.mount, cfg.path);
        out.fs_ready = scan.fs_ready;
        out.has_tracks = scan.has_tracks;
        out.status.assign(scan.status.view());
        out.mount_status.assign(scan.mount_status.view());
        out.tracks = scan.tracks;
    }

    TrackScanResult scan_tracks_default() {
        TrackScanResult out{};
        scan_tracks_default_into(out);
        return out;
    }

    void scan_storage_into(StorageState& out) {
        out = {};
        scan_tracks_default_into(out);
        out.labels_ready = false;
    }

    StorageState scan_storage() {
        StorageState out{};
        scan_storage_into(out);
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

    void ensure_track_labels(StorageState& state) {
        if (state.labels_ready) return;
        state.track_labels = {};
        state.track_titles = {};
        state.track_subtitles = {};
        for (std::size_t i = 0; i < state.tracks.size(); ++i) {
            const auto path_view = state.tracks[i].view();
            std::string_view base{path_view};
            const auto pos = base.find_last_of("/\\");
            if (pos != std::string_view::npos) base = base.substr(pos + 1);
            TrackLabel label{};
            label.assign(base);
            if (!state.track_labels.push_back(label)) break;
            std::string_view ext{};
            const auto dot = base.find_last_of('.');
            if (dot != std::string_view::npos && dot + 1 < base.size()) {
                ext = base.substr(dot + 1);
            }
            TrackTitle title{};
            title.assign(base);
            if (title.empty()) {
                title.assign("Unknown Track");
            }
            if (!state.track_titles.push_back(title)) break;
            if (!ext.empty()) {
                char upper_buf[32]{};
                const std::size_t count = std::min<std::size_t>(ext.size(), sizeof(upper_buf) - 1);
                for (std::size_t j = 0; j < count; ++j) {
                    upper_buf[j] = static_cast<char>(std::toupper(static_cast<unsigned char>(ext[j])));
                }
                TrackSubtitle subtitle{};
                subtitle.assign(std::string_view(upper_buf, count));
                if (!state.track_subtitles.push_back(subtitle)) break;
            } else {
                TrackSubtitle subtitle{};
                subtitle.assign("UNKNOWN");
                if (!state.track_subtitles.push_back(subtitle)) break;
            }
        }
        state.labels_ready = true;
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

    bool check_track_ready(
        std::string_view vfs_path,
        FixedString<product_config::status_text_capacity>& out_status) {
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

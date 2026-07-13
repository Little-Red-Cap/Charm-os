module;
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cctype>
#include <string_view>

#ifndef CHARM_AUDIO_ENABLE_FLAC
#define CHARM_AUDIO_ENABLE_FLAC 1
#endif
#ifndef CHARM_AUDIO_ENABLE_MP3
#define CHARM_AUDIO_ENABLE_MP3 1
#endif

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
import player.product_policy;
import player.media_library;
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
        MediaLibraryStats media_stats{};
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
        const MediaLibraryStats* media_stats{nullptr};
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
            if constexpr (product_policy::storage_default) {
                return StorageConfig{&fs_utils::mount_fatfs_from_vhd,
                                     product_config::default_storage_image_path};
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
        if constexpr (requires { out.media_stats; }) {
            out.media_stats.track_count = scan.tracks.size();
            out.media_stats.track_truncated = scan.track_truncated;
            out.media_stats.dir_truncated = scan.dir_truncated;
        }
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
        out.media_stats = &state.media_stats;
        return out;
    }

    void ensure_track_labels(StorageState& state) {
        if (state.labels_ready) return;
        state.track_labels = {};
        state.track_titles = {};
        state.track_subtitles = {};
        for (std::size_t i = 0; i < state.tracks.size(); ++i) {
            const auto track = derive_media_track(state.tracks[i].view());
            TrackLabel label{};
            label.assign(track.file_name);
            if (!state.track_labels.push_back(label)) break;
            TrackTitle title{};
            title.assign(track.title);
            if (title.empty()) {
                title.assign("Unknown Track");
            }
            if (!state.track_titles.push_back(title)) break;
            TrackSubtitle subtitle{};
            subtitle.assign(track.has_artist ? track.artist : track.album);
            if (!state.track_subtitles.push_back(subtitle)) break;
        }
        state.media_stats.track_count = state.tracks.size();
        state.media_stats.album_count = 0;
        state.media_stats.artist_count = 0;
        for (std::size_t i = 0; i < state.tracks.size(); ++i) {
            const auto track = derive_media_track(state.tracks[i].view());
            bool seen_album = false;
            bool seen_artist = false;
            for (std::size_t j = 0; j < i; ++j) {
                const auto prev = derive_media_track(state.tracks[j].view());
                if (compare_media_text_ci(track.album, prev.album) == 0) {
                    seen_album = true;
                }
                if (compare_media_text_ci(track.artist, prev.artist) == 0) {
                    seen_artist = true;
                }
            }
            if (!seen_album) ++state.media_stats.album_count;
            if (!seen_artist) ++state.media_stats.artist_count;
        }
        state.labels_ready = true;
    }

    bool is_playback_supported_track_path(std::string_view path) noexcept {
        return media_path_has_decoder_support(path);
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
        if (!is_playback_supported_track_path(vfs_path)) {
            out_status.assign("Unsupported format");
            return false;
        }
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

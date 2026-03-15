module;
#include <cstddef>

export module player.storage;

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
}

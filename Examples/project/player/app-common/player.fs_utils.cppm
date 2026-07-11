module;
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

export module player.fs_utils;

import service.fixed_vector;
import player.fixed_string;
import player.host_features;
import audio.source.fs;
import fs_core;
import fs_errno;
import fs_block;
import fs_stream;
import fs_path;
import fs_mal_block;
import fs_mal_file;
import fs_fatfs;
import fs_vfs;
import util.core;
import player.media_library;

export namespace player::fs_utils {
    namespace detail {
        constexpr std::size_t kMaxScanDirs = 64;
        constexpr bool kFsLogEnabled = player::host_features::fs_log;
#if defined(CHARM_PLAYER_COVER_DEBUG)
        constexpr bool kCoverLogEnabled = true;
#else
        constexpr bool kCoverLogEnabled = false;
#endif
    }

    namespace detail {
        void dump_indent(int depth) {
            if (!kFsLogEnabled) return;
            for (int i = 0; i < depth; ++i) {
                std::printf("  ");
            }
        }

        void dump_name_escaped(std::string_view name) {
            if (!kFsLogEnabled && !kCoverLogEnabled) return;
            for (unsigned char ch : name) {
                if (std::isprint(ch)) {
                    std::printf("%c", static_cast<char>(ch));
                } else {
                    std::printf("\\x%02X", static_cast<unsigned int>(ch));
                }
            }
        }

        bool join_path(FixedString<260>& out,
                       std::string_view dir,
                       std::string_view name) noexcept {
            out.clear();
            if (dir.empty() || dir == "/") {
                out.assign("/");
            } else {
                out.assign(dir);
                if (out.back() != '/') {
                    out.append("/");
                }
            }
            return out.append(name);
        }
    }
    namespace detail {
        struct MbrPartition {
            std::uint8_t status;
            std::uint8_t chs_first[3];
            std::uint8_t type;
            std::uint8_t chs_last[3];
            std::uint32_t lba_first;
            std::uint32_t sectors;
        };

        std::uint32_t find_fat_partition_lba(const std::array<std::uint8_t, 512>& sector0) {
            if (sector0[510] != 0x55 || sector0[511] != 0xAA) return 0;
            const auto* parts = reinterpret_cast<const MbrPartition*>(sector0.data() + 446);
            for (int i = 0; i < 4; ++i) {
                const auto& p = parts[i];
                if (p.type == 0x0B || p.type == 0x0C) {
                    return p.lba_first;
                }
            }
            return 0;
        }

        constexpr std::array<std::string_view, 6> kCoverNames{
            "cover.jpg",
            "cover.png",
            "cover.bmp",
            "folder.jpg",
            "folder.png",
            "folder.bmp",
        };

        int match_cover_name(std::string_view name) {
            for (std::size_t i = 0; i < kCoverNames.size(); ++i) {
                const auto cand = kCoverNames[i];
                if (name.size() != cand.size()) continue;
                bool eq = true;
                for (std::size_t j = 0; j < cand.size(); ++j) {
                    const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(name[j])));
                    const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(cand[j])));
                    if (a != b) {
                        eq = false;
                        break;
                    }
                }
                if (eq) return static_cast<int>(i);
            }
            return -1;
        }

        template <typename OutList, typename SubList>
        struct TrackListContext {
            std::string_view dir{};
            OutList* out{nullptr};
            SubList* subdirs{nullptr};
        };

        template <typename OutList, typename SubList>
        fs::Status collect_track(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
            auto* info = static_cast<TrackListContext<OutList, SubList>*>(ctx);
            auto* out_list = info ? info->out : nullptr;
            auto* subdirs_list = info ? info->subdirs : nullptr;
            if (!info || !out_list) return fs::Status{fs::Errc::inval};
            if (entry.type == fs::NodeType::dir) {
                if (!subdirs_list) return fs::Status{fs::Errc::ok};
                FixedString<260> path;
                if (detail::join_path(path, info->dir, entry.name)) {
                    if (!subdirs_list->push_back(path)) {
                        return fs::Status{fs::Errc::nomem};
                    }
                }
                return fs::Status{fs::Errc::ok};
            }
            if (entry.type != fs::NodeType::file) return fs::Status{fs::Errc::ok};
            const auto format = player::media_track_format(entry.name);
            if (format.empty()) {
                if (detail::kFsLogEnabled) {
                    bool has_non_ascii = false;
                    for (unsigned char ch : entry.name) {
                        if (ch >= 0x80u || !std::isprint(ch)) {
                            has_non_ascii = true;
                            break;
                        }
                    }
                    if (has_non_ascii) {
                        std::printf("[fs] skip non-audio file: ");
                        detail::dump_name_escaped(entry.name);
                        std::printf("\n");
                    }
                }
                return fs::Status{fs::Errc::ok};
            }

            FixedString<260> path;
            if (detail::join_path(path, info->dir, entry.name)) {
                if (!out_list->push_back(path)) {
                    return fs::Status{fs::Errc::nomem};
                }
            }
            return fs::Status{fs::Errc::ok};
        }

        struct OffsetDevice {
            fs::BlockDevice* base{nullptr};
            std::uint32_t lba_offset{0};
            fs::BlockDevice device{};

            static fs::Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> out) noexcept {
                auto* self = static_cast<OffsetDevice*>(ctx);
                return self->base->read(self->base->ctx, lba + self->lba_offset, out);
            }
            static fs::Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> in) noexcept {
                auto* self = static_cast<OffsetDevice*>(ctx);
                return self->base->write(self->base->ctx, lba + self->lba_offset, in);
            }
            static fs::Status erase_impl(void* ctx, util::u64 lba, util::u64 count) noexcept {
                auto* self = static_cast<OffsetDevice*>(ctx);
                return self->base->erase(self->base->ctx, lba + self->lba_offset, count);
            }
            static fs::Status flush_impl(void* ctx) noexcept {
                auto* self = static_cast<OffsetDevice*>(ctx);
                return self->base->flush ? self->base->flush(self->base->ctx) : fs::Status{fs::Errc::ok};
            }

            void init(fs::BlockDevice& dev, std::uint32_t offset) noexcept {
                base = &dev;
                lba_offset = offset;
                device.ctx = this;
                device.read = &OffsetDevice::read_impl;
                device.write = &OffsetDevice::write_impl;
                device.erase = &OffsetDevice::erase_impl;
                device.flush = &OffsetDevice::flush_impl;
                device.block_size = dev.block_size;
                device.block_count = dev.block_count > offset ? (dev.block_count - offset) : 0;
            }
        };
    }

    const char* fs_err_text(fs::Errc err) {
        switch (err) {
        case fs::Errc::ok: return "ok";
        case fs::Errc::perm: return "perm";
        case fs::Errc::noent: return "noent";
        case fs::Errc::exist: return "exist";
        case fs::Errc::io: return "io";
        case fs::Errc::busy: return "busy";
        case fs::Errc::inval: return "inval";
        case fs::Errc::nametoolong: return "nametoolong";
        case fs::Errc::nosys: return "nosys";
        case fs::Errc::nomem: return "nomem";
        case fs::Errc::notsup: return "notsup";
        case fs::Errc::rofs: return "rofs";
        case fs::Errc::timeout: return "timeout";
        case fs::Errc::again: return "again";
        default:
            break;
        }
        return "unknown";
    }

    template <typename OutList, typename SubList>
    bool collect_tracks_from_dir(std::string_view dir,
                                 OutList& out,
                                 SubList* subdirs,
                                 fs::Status& out_status) {
        detail::TrackListContext<OutList, SubList> ctx{dir, &out, subdirs};
        out_status = fs::vfs_list(dir, &ctx, &detail::collect_track<OutList, SubList>);
        return out_status && out.size() > 0;
    }

    template <typename OutList>
    bool collect_tracks_from_dir(std::string_view dir,
                                 OutList& out,
                                 std::nullptr_t,
                                 fs::Status& out_status) {
        return collect_tracks_from_dir<OutList, OutList>(dir, out, static_cast<OutList*>(nullptr), out_status);
    }

    bool find_cover_for_track(std::string_view track_path, FixedString<260>& out_path) {
        out_path.clear();
        auto norm = fs::normalize(track_path);
        fs::PathView p{norm.data, norm.size};
        auto parts = fs::split_last(p);
        fs::PathView dir = parts.first;
        if (!dir.data) return false;

        FixedString<260> dir_path;
        dir_path.assign(std::string_view(dir.data, dir.size));
        if (dir_path.empty()) {
            dir_path.assign("/");
        }
        if (detail::kCoverLogEnabled) {
            std::printf("[cover] scan dir: %.*s\n",
                        static_cast<int>(dir_path.size()), dir_path.c_str());
        }

        struct CoverCtx {
            std::string_view dir;
            FixedString<260>* out;
            int best_index;
        };

        struct CoverCollector {
            static fs::Status collect(void* ctx_ptr, const fs::MountOps::ListEntry& entry) noexcept {
                auto* info = static_cast<CoverCtx*>(ctx_ptr);
                if (!info || !info->out) return fs::Status{fs::Errc::inval};
                if (entry.type != fs::NodeType::file) return fs::Status{fs::Errc::ok};
                const int idx = detail::match_cover_name(entry.name);
                if (detail::kCoverLogEnabled && idx >= 0) {
                    std::printf("[cover] candidate: ");
                    detail::dump_name_escaped(entry.name);
                    std::printf(" idx=%d\n", idx);
                }
                if (idx < 0) return fs::Status{fs::Errc::ok};
                if (info->best_index >= 0 && idx >= info->best_index) return fs::Status{fs::Errc::ok};
                FixedString<260> path;
                if (!detail::join_path(path, info->dir, entry.name)) {
                    return fs::Status{fs::Errc::ok};
                }
                info->out->assign(path.view());
                info->best_index = idx;
                return fs::Status{fs::Errc::ok};
            }
        };

        const auto dir_view = dir_path.view();
        CoverCtx ctx{dir_view, &out_path, -1};
        fs::Status st = fs::vfs_list(dir_view, &ctx, &CoverCollector::collect);
        return st && !out_path.empty();
    }

    bool dump_fs_tree(std::string_view dir, int depth, int max_depth) {
        if (!detail::kFsLogEnabled) return true;
        if (depth > max_depth) return true;
        struct DumpCtx {
            std::string_view dir;
            service::FixedVector<FixedString<260>, detail::kMaxScanDirs>* subdirs;
            int depth;
        };
        service::FixedVector<FixedString<260>, detail::kMaxScanDirs> subdirs;
        DumpCtx ctx{dir, &subdirs, depth};
        fs::Status st = fs::vfs_list(dir, &ctx, [](void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
            auto* info = static_cast<DumpCtx*>(ctx);
            if (!info || !info->subdirs) return fs::Status{fs::Errc::inval};
            detail::dump_indent(info->depth);
            detail::dump_name_escaped(entry.name);
            std::printf("%s\n", entry.type == fs::NodeType::dir ? "/" : "");
            if (entry.type == fs::NodeType::dir) {
                FixedString<260> path;
                if (detail::join_path(path, info->dir, entry.name)) {
                    (void)info->subdirs->push_back(path);
                }
            }
            return fs::Status{fs::Errc::ok};
        });
        if (!st) return false;
        for (std::size_t i = 0; i < subdirs.size(); ++i) {
            if (!dump_fs_tree(subdirs[i].view(), depth + 1, max_depth)) return false;
        }
        return true;
    }

    fs::Status mount_fatfs_from_vhd(const char* path) {
        if (!path || !*path) return fs::Status{fs::Errc::inval};
        static fs::MalFile file_dev;
        static detail::OffsetDevice part_dev;
        static fs::FatFsMount fat;

        auto st = file_dev.open(path, 512);
        if (!st) return st;

        std::array<std::uint8_t, 512> sector0{};
        auto& base = file_dev.block_file().device();
        st = base.read(base.ctx, 0, std::span<util::u8>(sector0));
        if (!st) return st;

        const auto lba = detail::find_fat_partition_lba(sector0);
        part_dev.init(base, lba);
        st = fat.mount(part_dev.device, false);
        if (!st) return st;

        fs::clear_mounts();
        (void)fs::add_mount("/", fat.mount_point());
        return fs::Status{fs::Errc::ok};
    }

    bool fs_seek_selftest(const char* path) {
        if (!path || !*path) return false;
        audio::FsDataSource src;
        if (!src.open(path)) return false;
        const auto size = src.size();
        if (!size || *size < 32) return true;
        auto pos = src.tell();
        if (!pos || *pos != 0) return false;
        if (!src.seek(0, SEEK_SET)) return false;
        pos = src.tell();
        if (!pos || *pos != 0) return false;
        if (!src.seek(16, SEEK_CUR)) return false;
        pos = src.tell();
        if (!pos || *pos != 16) return false;
        if (!src.seek(-8, SEEK_END)) return false;
        pos = src.tell();
        if (!pos || *pos != (*size - 8)) return false;
        return true;
    }
}

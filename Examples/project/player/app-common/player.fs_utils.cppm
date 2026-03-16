module;
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <vector>

export module player.fs_utils;

import audio.source.fs;
import fs_core;
import fs_errno;
import fs_block;
import fs_stream;
import fs_mal_block;
import fs_mal_file;
import fs_fatfs;
import fs_vfs;
import util.core;

export namespace player::fs_utils {
    namespace detail {
        void dump_indent(int depth) {
            for (int i = 0; i < depth; ++i) {
                std::printf("  ");
            }
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

        bool has_audio_ext(std::string_view name) {
            const auto dot = name.find_last_of('.');
            if (dot == std::string_view::npos || dot + 1 >= name.size()) return false;
            const auto ext = name.substr(dot + 1);
            if (ext.size() == 3) {
                const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
                const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
                if (a == 'm' && b == 'p' && c == '3') return true;
                if (a == 'w' && b == 'a' && c == 'v') return true;
                if (a == 'f' && b == 'l' && c == 'a') return true;
            }
            if (ext.size() == 4) {
                const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[0])));
                const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[1])));
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[2])));
                const char d = static_cast<char>(std::tolower(static_cast<unsigned char>(ext[3])));
                if (a == 'f' && b == 'l' && c == 'a' && d == 'c') return true;
            }
            return false;
        }

        struct TrackListContext {
            std::string_view dir{};
            std::vector<std::string>* out{nullptr};
            std::vector<std::string>* subdirs{nullptr};
        };

        fs::Status collect_track(void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
            auto* info = static_cast<TrackListContext*>(ctx);
            if (!info || !info->out) return fs::Status{fs::Errc::inval};
            if (entry.type == fs::NodeType::dir) {
                if (!info->subdirs) return fs::Status{fs::Errc::ok};
                std::string path;
                if (info->dir.empty() || info->dir == "/") {
                    path = "/";
                } else {
                    path.assign(info->dir.begin(), info->dir.end());
                    if (!path.empty() && path.back() != '/') path.push_back('/');
                }
                path.append(entry.name.begin(), entry.name.end());
                info->subdirs->push_back(path);
                return fs::Status{fs::Errc::ok};
            }
            if (entry.type != fs::NodeType::file) return fs::Status{fs::Errc::ok};
            if (!has_audio_ext(entry.name)) return fs::Status{fs::Errc::ok};

            std::string path;
            if (info->dir.empty() || info->dir == "/") {
                path = "/";
            } else {
                path.assign(info->dir.begin(), info->dir.end());
                if (!path.empty() && path.back() != '/') path.push_back('/');
            }
            path.append(entry.name.begin(), entry.name.end());
            info->out->push_back(path);
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

    bool collect_tracks_from_dir(std::string_view dir,
                                 std::vector<std::string>& out,
                                 std::vector<std::string>* subdirs,
                                 fs::Status& out_status) {
        detail::TrackListContext ctx{dir, &out, subdirs};
        out_status = fs::vfs_list(dir, &ctx, &detail::collect_track);
        return out_status && !out.empty();
    }

    bool dump_fs_tree(std::string_view dir, int depth, int max_depth) {
        if (depth > max_depth) return true;
        struct DumpCtx {
            std::string_view dir;
            std::vector<std::string>* subdirs;
            int depth;
        };
        std::vector<std::string> subdirs;
        DumpCtx ctx{dir, &subdirs, depth};
        fs::Status st = fs::vfs_list(dir, &ctx, [](void* ctx, const fs::MountOps::ListEntry& entry) noexcept {
            auto* info = static_cast<DumpCtx*>(ctx);
            if (!info || !info->subdirs) return fs::Status{fs::Errc::inval};
            detail::dump_indent(info->depth);
            std::printf("%.*s%s\n",
                        static_cast<int>(entry.name.size()),
                        entry.name.data(),
                        entry.type == fs::NodeType::dir ? "/" : "");
            if (entry.type == fs::NodeType::dir) {
                std::string path;
                if (info->dir.empty() || info->dir == "/") {
                    path = "/";
                } else {
                    path.assign(info->dir.begin(), info->dir.end());
                    if (!path.empty() && path.back() != '/') path.push_back('/');
                }
                path.append(entry.name.begin(), entry.name.end());
                info->subdirs->push_back(path);
            }
            return fs::Status{fs::Errc::ok};
        });
        if (!st) return false;
        for (const auto& sub : subdirs) {
            if (!dump_fs_tree(sub, depth + 1, max_depth)) return false;
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

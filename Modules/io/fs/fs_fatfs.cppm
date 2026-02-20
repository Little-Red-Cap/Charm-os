module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

#if CHARM_USE_FATFS
#include "ff.h"
#include "diskio.h"
#endif

export module fs_fatfs;

import util.core;
import fs_core;
import fs_errno;
import fs_stream;
import fs_path;
import fs_block;
import fs_mal;

#if CHARM_USE_FATFS
export namespace fs {
    void fatfs_register_block_device(BlockDevice* dev, util::u8 pdrv = 0) noexcept;
    void fatfs_set_cache(util::u8* buf, util::usize size) noexcept;

    struct FatFsFileSlot {
        bool used{false};
        FIL file{};
    };

    inline Err err_from_fr(FRESULT fr) noexcept {
        switch (fr) {
        case FR_OK: return Err::ok;
        case FR_NO_FILE:
        case FR_NO_PATH:
            return Err::noent;
        case FR_INVALID_NAME:
        case FR_INVALID_OBJECT:
        case FR_INVALID_PARAMETER:
            return Err::inval;
        case FR_EXIST:
            return Err::exist;
        case FR_DENIED:
            return Err::perm;
        case FR_WRITE_PROTECTED:
            return Err::rofs;
        case FR_NOT_READY:
        case FR_DISK_ERR:
        case FR_INT_ERR:
            return Err::io;
        case FR_NO_FILESYSTEM:
            return Err::noent;
        case FR_TIMEOUT:
            return Err::timeout;
        case FR_NOT_ENOUGH_CORE:
            return Err::nomem;
        default:
            return Err::io;
        }
    }

    inline Status status_from_fr(FRESULT fr) noexcept {
        return Status{err_from_fr(fr)};
    }

    class FatFsMount {
    public:
        static constexpr util::usize max_files =
#ifdef CHARM_FATFS_MAX_FILES
            static_cast<util::usize>(CHARM_FATFS_MAX_FILES);
#else
            8;
#endif

        FatFsMount() = default;
        FatFsMount(const FatFsMount&) = delete;
        FatFsMount& operator=(const FatFsMount&) = delete;

        void set_path_buffers(std::span<TCHAR> buf0, std::span<TCHAR> buf1 = {}) noexcept {
            path_bufs_[0] = buf0.empty() ? std::span<TCHAR>{path_buf_store_[0]} : buf0;
            path_bufs_[1] = buf1.empty() ? std::span<TCHAR>{path_buf_store_[1]} : buf1;
            path_buf_next_ = 0;
        }

        void set_file_slots(std::span<FatFsFileSlot> slots) noexcept {
            if (slots.empty()) {
                slots_ = std::span<FatFsFileSlot>{files_};
            } else {
                slots_ = slots;
            }
            clear_slots();
        }

        Status mount(BlockDevice& dev, bool format_if_needed = false, util::u8 pdrv = 0) noexcept {
            dev_ = &dev;
            mal_ = nullptr;
            clear_slots();
            fatfs_register_block_device(dev_, pdrv);
            auto fr = f_mount(&fs_, "", 1);
            if (fr == FR_NO_FILESYSTEM && format_if_needed) {
#if defined(FF_USE_MKFS) && FF_USE_MKFS
                std::array<util::u8, 4096> work{};
#if defined(MKFS_PARM)
                MKFS_PARM opt{};
                opt.fmt = FM_FAT32;
                fr = f_mkfs("", &opt, work.data(), static_cast<UINT>(work.size()));
#else
                fr = f_mkfs("", FM_FAT32, 0, work.data(), static_cast<UINT>(work.size()));
#endif
                if (fr != FR_OK) return status_from_fr(fr);
                fr = f_mount(&fs_, "", 1);
#else
                return Status{Err::notsup};
#endif
            }
            if (fr != FR_OK) return status_from_fr(fr);
            mount_.ops = &ops_;
            mount_.data = this;
            clear_dirty(&mount_);
            return Status{Err::ok};
        }

        Status mount(BlockDevice& dev, std::span<util::u8> cache, bool format_if_needed = false,
            util::u8 pdrv = 0) noexcept {
            fatfs_set_cache(cache.data(), cache.size());
            return mount(dev, format_if_needed, pdrv);
        }

        Status mount(MalDevice& dev, bool format_if_needed = false, util::u8 pdrv = 0) noexcept {
            mal_ = &dev;
            mal_to_block(dev, mal_block_);
            return mount(mal_block_, format_if_needed, pdrv);
        }

        Status mount(MalDevice& dev, std::span<util::u8> cache, bool format_if_needed = false,
            util::u8 pdrv = 0) noexcept {
            mal_ = &dev;
            mal_to_block(dev, mal_block_);
            return mount(mal_block_, cache, format_if_needed, pdrv);
        }

        Status unmount(bool force = false) noexcept {
            (void)force;
            (void)f_mount(nullptr, "", 1);
            dev_ = nullptr;
            mal_ = nullptr;
            mal_block_ = {};
            mount_.ops = nullptr;
            mount_.data = nullptr;
            return Status{Err::ok};
        }

        Mount* mount_point() noexcept { return &mount_; }

        Status close(File& f) noexcept {
            auto* slot = static_cast<FatFsFileSlot*>(f.node.data);
            if (!slot || !slot->used) return Status{Err::inval};
            const auto fr = f_close(&slot->file);
            slot->used = false;
            f.node.data = nullptr;
            f.node.ops = nullptr;
            f.mount = nullptr;
            return status_from_fr(fr);
        }

    private:
        static MountOps ops_;

        static Status open_impl(Mount* m, std::string_view path, File& out, OpenFlags flags) noexcept {
            if (!m) return Status{Err::inval};
            auto* self = static_cast<FatFsMount*>(m->data);
            if (!self) return Status{Err::inval};
            auto* slot = self->alloc_slot();
            if (!slot) return Status{Err::nomem};

            auto buf = self->build_path(path);
            if (!buf) {
                self->free_slot(slot);
                return Status{Err::nametoolong};
            }
            const auto* cpath = buf->data();
            BYTE mode = 0;
            const bool want_read = has_flag(flags, OpenFlags::read);
            const bool want_write = has_flag(flags, OpenFlags::write);
            const bool want_create = has_flag(flags, OpenFlags::create);
            const bool want_trunc = has_flag(flags, OpenFlags::trunc);

            if ((want_create || want_trunc) && !want_write) {
                self->free_slot(slot);
                return Status{Err::perm};
            }

            mode |= want_read ? FA_READ : 0;
            mode |= want_write ? FA_WRITE : 0;
            if (!want_read && !want_write) mode |= FA_READ;

            if (want_trunc && want_create) {
                mode |= FA_CREATE_ALWAYS;
            } else if (want_create) {
                mode |= FA_OPEN_ALWAYS;
            } else {
                mode |= FA_OPEN_EXISTING;
            }

            auto fr = f_open(&slot->file, cpath, mode);
            if (fr != FR_OK) {
                self->free_slot(slot);
                return status_from_fr(fr);
            }
            if (want_trunc && !want_create) {
                fr = f_truncate(&slot->file);
                if (fr != FR_OK) {
                    self->free_slot(slot);
                    return status_from_fr(fr);
                }
                (void)f_lseek(&slot->file, 0);
            }

            out.node.type = NodeType::file;
            out.node.ops = &self->node_ops_;
            out.node.data = slot;
            out.node.size = static_cast<util::i64>(f_size(&slot->file));
            out.node.offset = static_cast<util::i64>(f_tell(&slot->file));
            out.mount = m;
            return Status{Err::ok};
        }

        static Status flush_impl(Mount* m) noexcept {
            if (!m) return Status{Err::inval};
            clear_dirty(m);
            return Status{Err::ok};
        }

        static Status unmount_impl(Mount* m, bool force) noexcept {
            if (!m) return Status{Err::inval};
            auto* self = static_cast<FatFsMount*>(m->data);
            if (!self) return Status{Err::inval};
            return self->unmount(force);
        }

        static Status unlink_impl(Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<FatFsMount*>(m ? m->data : nullptr);
            if (!self) return Status{Err::inval};
            auto buf = self->build_path(path);
            if (!buf) return Status{Err::nametoolong};
            return status_from_fr(f_unlink(buf->data()));
        }

        static Status rename_impl(Mount* m, std::string_view from, std::string_view to) noexcept {
            auto* self = static_cast<FatFsMount*>(m ? m->data : nullptr);
            if (!self) return Status{Err::inval};
            auto bfrom = self->build_path(from);
            auto bto = self->build_path(to);
            if (!bfrom || !bto) return Status{Err::nametoolong};
            return status_from_fr(f_rename(bfrom->data(), bto->data()));
        }

        static Status truncate_impl(Mount* m, std::string_view path, util::u64 size) noexcept {
            auto* self = static_cast<FatFsMount*>(m ? m->data : nullptr);
            if (!self) return Status{Err::inval};
            auto buf = self->build_path(path);
            if (!buf) return Status{Err::nametoolong};
            FIL fil{};
            auto fr = f_open(&fil, buf->data(), FA_WRITE);
            if (fr != FR_OK) return status_from_fr(fr);
            fr = f_lseek(&fil, static_cast<FSIZE_t>(size));
            if (fr == FR_OK) fr = f_truncate(&fil);
            (void)f_close(&fil);
            return status_from_fr(fr);
        }

        static Status mkdir_impl(Mount* m, std::string_view path) noexcept {
            auto* self = static_cast<FatFsMount*>(m ? m->data : nullptr);
            if (!self) return Status{Err::inval};
            auto buf = self->build_path(path);
            if (!buf) return Status{Err::nametoolong};
            return status_from_fr(f_mkdir(buf->data()));
        }

        static Status list_impl(Mount* m, std::string_view path, void* ctx, MountOps::ListFn fn) noexcept {
            if (!fn) return Status{Err::inval};
            auto* self = static_cast<FatFsMount*>(m ? m->data : nullptr);
            if (!self) return Status{Err::inval};
            auto buf = self->build_path(path);
            if (!buf) return Status{Err::nametoolong};

            DIR dir{};
            auto fr = f_opendir(&dir, buf->data());
            if (fr != FR_OK) return status_from_fr(fr);
            FILINFO info{};
#if defined(FF_USE_LFN) && FF_USE_LFN
            std::array<TCHAR, max_path> lfn{};
            info.lfname = lfn.data();
            info.lfsize = static_cast<UINT>(lfn.size());
#endif
            while (true) {
                fr = f_readdir(&dir, &info);
                if (fr != FR_OK) {
                    (void)f_closedir(&dir);
                    return status_from_fr(fr);
                }
                if (info.fname[0] == '\0') break;
                const char* name = info.fname;
#if defined(FF_USE_LFN) && FF_USE_LFN
#if defined(FF_LFN_UNICODE) && FF_LFN_UNICODE
#if (FF_LFN_UNICODE == 1)
                if (info.lfname && info.lfname[0] != 0) {
                    constexpr util::usize lfn_utf8_cap =
#if defined(FF_MAX_LFN)
                        static_cast<util::usize>(FF_MAX_LFN) * 4 + 1;
#else
                        256 * 4 + 1;
#endif
                    std::array<char, lfn_utf8_cap> fname_utf8{};
                    const auto written = utf16_to_utf8(info.lfname, fname_utf8.data(), fname_utf8.size());
                    if (written > 0) {
                        fname_utf8[std::min(written, fname_utf8.size() - 1)] = '\0';
                        name = fname_utf8.data();
                    }
                }
#else
                if (info.lfname && info.lfname[0] != 0) {
                    name = info.lfname;
                }
#endif
#endif
#endif
                MountOps::ListEntry entry{};
                entry.name = std::string_view{name};
                entry.size = static_cast<util::u64>(info.fsize);
                entry.type = (info.fattrib & AM_DIR) ? NodeType::dir : NodeType::file;
                auto st = fn(ctx, entry);
                if (!st) {
                    (void)f_closedir(&dir);
                    return st;
                }
            }
            (void)f_closedir(&dir);
            return Status{Err::ok};
        }

        static Status read_impl(Node& n, std::span<util::u8> buf) noexcept {
            auto* slot = static_cast<FatFsFileSlot*>(n.data);
            if (!slot || !slot->used) return Status{Err::inval};
            UINT read = 0;
            auto fr = f_read(&slot->file, buf.data(), static_cast<UINT>(buf.size()), &read);
            if (fr != FR_OK) return status_from_fr(fr);
            n.offset = static_cast<util::i64>(f_tell(&slot->file));
            return Status{Err::ok};
        }

        static Status write_impl(Node& n, std::span<const util::u8> buf) noexcept {
            auto* slot = static_cast<FatFsFileSlot*>(n.data);
            if (!slot || !slot->used) return Status{Err::inval};
            UINT written = 0;
            auto fr = f_write(&slot->file, buf.data(), static_cast<UINT>(buf.size()), &written);
            if (fr != FR_OK) return status_from_fr(fr);
            n.offset = static_cast<util::i64>(f_tell(&slot->file));
            n.size = static_cast<util::i64>(f_size(&slot->file));
            return Status{Err::ok};
        }

        static Status seek_impl(Node& n, util::i64 off) noexcept {
            if (off < 0) return Status{Err::inval};
            auto* slot = static_cast<FatFsFileSlot*>(n.data);
            if (!slot || !slot->used) return Status{Err::inval};
            auto fr = f_lseek(&slot->file, static_cast<FSIZE_t>(off));
            if (fr != FR_OK) return status_from_fr(fr);
            n.offset = static_cast<util::i64>(f_tell(&slot->file));
            return Status{Err::ok};
        }

        static Status file_flush_impl(Node& n) noexcept {
            auto* slot = static_cast<FatFsFileSlot*>(n.data);
            if (!slot || !slot->used) return Status{Err::inval};
            return status_from_fr(f_sync(&slot->file));
        }

        static Status close_impl(Node& n) noexcept {
            auto* slot = static_cast<FatFsFileSlot*>(n.data);
            if (!slot || !slot->used) return Status{Err::inval};
            const auto fr = f_close(&slot->file);
            slot->used = false;
            std::memset(&slot->file, 0, sizeof(slot->file));
            return status_from_fr(fr);
        }

        FatFsFileSlot* alloc_slot() noexcept {
            for (auto& slot : slots_) {
                if (!slot.used) {
                    slot.used = true;
                    std::memset(&slot.file, 0, sizeof(slot.file));
                    return &slot;
                }
            }
            return nullptr;
        }

        void free_slot(FatFsFileSlot* slot) noexcept {
            if (slot) {
                slot->used = false;
                std::memset(&slot->file, 0, sizeof(slot->file));
            }
        }

        void clear_slots() noexcept {
            for (auto& slot : slots_) {
                slot.used = false;
                std::memset(&slot.file, 0, sizeof(slot.file));
            }
        }

#ifdef CHARM_FATFS_MAX_PATH
        static constexpr util::usize max_path = static_cast<util::usize>(CHARM_FATFS_MAX_PATH);
#else
        static constexpr util::usize max_path = 256;
#endif

#if defined(FF_LFN_UNICODE) && FF_LFN_UNICODE
        static bool append_utf8(util::u32 cp, char* out, util::usize cap, util::usize& pos) noexcept {
            if (cap == 0) return false;
            if (cp <= 0x7F) {
                if (pos + 1 >= cap) return false;
                out[pos++] = static_cast<char>(cp);
                return true;
            }
            if (cp <= 0x7FF) {
                if (pos + 2 >= cap) return false;
                out[pos++] = static_cast<char>(0xC0 | (cp >> 6));
                out[pos++] = static_cast<char>(0x80 | (cp & 0x3F));
                return true;
            }
            if (cp <= 0xFFFF) {
                if (pos + 3 >= cap) return false;
                out[pos++] = static_cast<char>(0xE0 | (cp >> 12));
                out[pos++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out[pos++] = static_cast<char>(0x80 | (cp & 0x3F));
                return true;
            }
            if (cp <= 0x10FFFF) {
                if (pos + 4 >= cap) return false;
                out[pos++] = static_cast<char>(0xF0 | (cp >> 18));
                out[pos++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out[pos++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out[pos++] = static_cast<char>(0x80 | (cp & 0x3F));
                return true;
            }
            return false;
        }

        static util::usize utf16_to_utf8(const TCHAR* in, char* out, util::usize cap) noexcept {
            if (!in || !out || cap == 0) return 0;
            util::usize pos = 0;
            util::usize idx = 0;
            while (in[idx] != 0) {
                util::u32 cp = static_cast<util::u32>(in[idx++]);
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                    const util::u32 hi = cp;
                    const util::u32 lo = static_cast<util::u32>(in[idx++]);
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000 + (((hi - 0xD800) << 10) | (lo - 0xDC00));
                    } else {
                        cp = 0xFFFD;
                    }
                } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                    cp = 0xFFFD;
                }
                if (!append_utf8(cp, out, cap, pos)) break;
            }
            if (pos < cap) out[pos] = '\0';
            return pos;
        }

        static bool utf8_to_utf16(std::string_view in, TCHAR* out, util::usize cap) noexcept {
            if (!out || cap == 0) return false;
            util::usize pos = 0;
            util::usize i = 0;
            while (i < in.size()) {
                util::u32 cp = 0xFFFD;
                const auto c0 = static_cast<unsigned char>(in[i]);
                if (c0 < 0x80) {
                    cp = c0;
                    i += 1;
                } else if ((c0 & 0xE0) == 0xC0 && i + 1 < in.size()) {
                    const auto c1 = static_cast<unsigned char>(in[i + 1]);
                    if ((c1 & 0xC0) == 0x80) {
                        cp = ((c0 & 0x1F) << 6) | (c1 & 0x3F);
                        if (cp < 0x80) cp = 0xFFFD;
                        i += 2;
                    } else {
                        i += 1;
                    }
                } else if ((c0 & 0xF0) == 0xE0 && i + 2 < in.size()) {
                    const auto c1 = static_cast<unsigned char>(in[i + 1]);
                    const auto c2 = static_cast<unsigned char>(in[i + 2]);
                    if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80) {
                        cp = ((c0 & 0x0F) << 12) | ((c1 & 0x3F) << 6) | (c2 & 0x3F);
                        if (cp < 0x800) cp = 0xFFFD;
                        i += 3;
                    } else {
                        i += 1;
                    }
                } else if ((c0 & 0xF8) == 0xF0 && i + 3 < in.size()) {
                    const auto c1 = static_cast<unsigned char>(in[i + 1]);
                    const auto c2 = static_cast<unsigned char>(in[i + 2]);
                    const auto c3 = static_cast<unsigned char>(in[i + 3]);
                    if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
                        cp = ((c0 & 0x07) << 18) | ((c1 & 0x3F) << 12) |
                             ((c2 & 0x3F) << 6) | (c3 & 0x3F);
                        if (cp < 0x10000 || cp > 0x10FFFF) cp = 0xFFFD;
                        i += 4;
                    } else {
                        i += 1;
                    }
                } else {
                    i += 1;
                }

                if (cp <= 0xFFFF) {
                    if (pos + 1 >= cap) break;
                    out[pos++] = static_cast<TCHAR>(cp);
                } else {
                    if (pos + 2 >= cap) break;
                    cp -= 0x10000;
                    out[pos++] = static_cast<TCHAR>(0xD800 + ((cp >> 10) & 0x3FF));
                    out[pos++] = static_cast<TCHAR>(0xDC00 + (cp & 0x3FF));
                }
            }
            if (pos < cap) out[pos] = 0;
            return pos + 1 < cap;
        }
#endif

        std::optional<std::span<TCHAR>> build_path(std::string_view path) noexcept {
            auto norm = normalize(path);
            std::string_view p{norm.data, norm.size};
            while (!p.empty() && p.front() == '/') p.remove_prefix(1);
            auto buf = next_path_buf();
            if (buf.empty()) return std::nullopt;
#if defined(FF_LFN_UNICODE) && FF_LFN_UNICODE
#if (FF_LFN_UNICODE == 1)
            if (!utf8_to_utf16(p, buf.data(), buf.size())) return std::nullopt;
#else
            if (p.size() + 1 > buf.size()) return std::nullopt;
            for (util::usize i = 0; i < p.size(); ++i) {
                buf[i] = static_cast<TCHAR>(p[i]);
            }
            buf[p.size()] = 0;
#endif
            return buf;
#else
            if (p.size() + 1 > buf.size()) return std::nullopt;
            std::memcpy(buf.data(), p.data(), p.size());
            buf[p.size()] = '\0';
            return buf;
#endif
        }

        BlockDevice* dev_{nullptr};
        MalDevice* mal_{nullptr};
        BlockDevice mal_block_{};
        FATFS fs_{};
        Mount mount_{};
        std::array<std::array<TCHAR, max_path>, 2> path_buf_store_{};
        std::array<std::span<TCHAR>, 2> path_bufs_{std::span<TCHAR>{path_buf_store_[0]},
            std::span<TCHAR>{path_buf_store_[1]}};
        util::u8 path_buf_next_{0};
        std::array<FatFsFileSlot, max_files> files_{};
        std::span<FatFsFileSlot> slots_{files_};
        NodeOps node_ops_{
            .read = &read_impl,
            .write = &write_impl,
            .seek = &seek_impl,
            .flush = &file_flush_impl,
            .close = &close_impl
        };

        std::span<TCHAR> next_path_buf() noexcept {
            const auto idx = static_cast<util::usize>(path_buf_next_ & 1u);
            path_buf_next_ = static_cast<util::u8>((path_buf_next_ + 1u) & 1u);
            return path_bufs_[idx];
        }
    };

    inline MountOps FatFsMount::ops_{
        .open = &FatFsMount::open_impl,
        .flush = &FatFsMount::flush_impl,
        .unmount = &FatFsMount::unmount_impl,
        .unlink = &FatFsMount::unlink_impl,
        .rename = &FatFsMount::rename_impl,
        .truncate = &FatFsMount::truncate_impl,
        .mkdir = &FatFsMount::mkdir_impl,
        .list = &FatFsMount::list_impl,
    };

    inline BlockDevice* g_fatfs_device = nullptr;
    inline BYTE g_fatfs_pdrv = 0;
    struct FatFsCache {
        util::u8* data{nullptr};
        util::usize size{0};
        util::u64 block_size{0};
        util::u64 sector{0};
        bool valid{false};
    };
    inline FatFsCache g_fatfs_cache{};

    inline void fatfs_register_block_device(BlockDevice* dev, util::u8 pdrv) noexcept {
        g_fatfs_device = dev;
        g_fatfs_pdrv = static_cast<BYTE>(pdrv);
        g_fatfs_cache.block_size = dev ? dev->block_size : 0;
        g_fatfs_cache.valid = false;
    }

    inline void fatfs_set_cache(util::u8* buf, util::usize size) noexcept {
        g_fatfs_cache.data = buf;
        g_fatfs_cache.size = size;
        g_fatfs_cache.valid = false;
    }

} // namespace fs

extern "C" {
    DSTATUS disk_initialize(BYTE pdrv) {
        if (pdrv != fs::g_fatfs_pdrv || !fs::g_fatfs_device) return STA_NOINIT;
        return 0;
    }

    DSTATUS disk_status(BYTE pdrv) {
        if (pdrv != fs::g_fatfs_pdrv || !fs::g_fatfs_device) return STA_NOINIT;
        return 0;
    }

    DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
        if (pdrv != fs::g_fatfs_pdrv || !fs::g_fatfs_device) return RES_NOTRDY;
        auto* dev = fs::g_fatfs_device;
        auto& cache = fs::g_fatfs_cache;
        if (count == 1 && cache.data && cache.block_size == dev->block_size &&
            cache.block_size <= cache.size) {
            const auto lba = static_cast<util::u64>(sector);
            if (cache.valid && cache.sector == lba) {
                std::memcpy(buff, cache.data, static_cast<util::usize>(cache.block_size));
                return RES_OK;
            }
            auto st = dev->read(dev->ctx, lba,
                std::span<util::u8>(cache.data, static_cast<util::usize>(cache.block_size)));
            if (!st) return RES_ERROR;
            cache.sector = lba;
            cache.valid = true;
            std::memcpy(buff, cache.data, static_cast<util::usize>(cache.block_size));
            return RES_OK;
        }
        const auto size = static_cast<util::usize>(count) * static_cast<util::usize>(dev->block_size);
        auto st = dev->read(dev->ctx, static_cast<util::u64>(sector),
            std::span<util::u8>(reinterpret_cast<util::u8*>(buff), size));
        return st ? RES_OK : RES_ERROR;
    }

    DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
        if (pdrv != fs::g_fatfs_pdrv || !fs::g_fatfs_device) return RES_NOTRDY;
        auto* dev = fs::g_fatfs_device;
        auto& cache = fs::g_fatfs_cache;
        if (count == 1 && cache.data && cache.block_size == dev->block_size &&
            cache.block_size <= cache.size) {
            const auto lba = static_cast<util::u64>(sector);
            auto st = dev->write(dev->ctx, lba,
                std::span<const util::u8>(reinterpret_cast<const util::u8*>(buff),
                    static_cast<util::usize>(cache.block_size)));
            if (!st) return RES_ERROR;
            std::memcpy(cache.data, buff, static_cast<util::usize>(cache.block_size));
            cache.sector = lba;
            cache.valid = true;
            return RES_OK;
        }
        const auto size = static_cast<util::usize>(count) * static_cast<util::usize>(dev->block_size);
        auto st = dev->write(dev->ctx, static_cast<util::u64>(sector),
            std::span<const util::u8>(reinterpret_cast<const util::u8*>(buff), size));
        if (cache.valid) {
            const auto lba = static_cast<util::u64>(sector);
            const auto end = lba + count;
            if (cache.sector >= lba && cache.sector < end) cache.valid = false;
        }
        return st ? RES_OK : RES_ERROR;
    }

    DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
        if (pdrv != fs::g_fatfs_pdrv || !fs::g_fatfs_device) return RES_NOTRDY;
        auto* dev = fs::g_fatfs_device;
        switch (cmd) {
        case CTRL_SYNC:
            return dev->flush ? (dev->flush(dev->ctx) ? RES_OK : RES_ERROR) : RES_OK;
        case GET_SECTOR_COUNT:
            *reinterpret_cast<DWORD*>(buff) = static_cast<DWORD>(dev->block_count);
            return RES_OK;
        case GET_SECTOR_SIZE:
            *reinterpret_cast<WORD*>(buff) = static_cast<WORD>(dev->block_size);
            return RES_OK;
        case GET_BLOCK_SIZE:
            *reinterpret_cast<DWORD*>(buff) = 1;
            return RES_OK;
        default:
            return RES_PARERR;
        }
    }

    DWORD get_fattime() {
        return 0;
    }
}

#else
export namespace fs {
    class FatFsMount {
    public:
        Status mount(BlockDevice&, bool = false) noexcept { return Status{Err::nosys}; }
        Status unmount(bool = false) noexcept { return Status{Err::nosys}; }
        Mount* mount_point() noexcept { return nullptr; }
        Status close(File&) noexcept { return Status{Err::nosys}; }
    };
}
#endif

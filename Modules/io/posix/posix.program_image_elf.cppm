module;

#include <cstddef>
#include <cstdint>
#include <cstring>

export module posix.program_image_elf;

import posix.program_image;
import module_core;
import util.core;
import util.error;

export namespace posix {
    inline ImageEntry addr_to_entry(modulex::Addr addr) noexcept {
        return reinterpret_cast<ImageEntry>(addr);
    }
    enum class ElfErrc : util::u8 {
        ok = 0,
        bad_magic,
        bad_class,
        bad_endian,
        bad_header,
        bad_phoff,
        bad_phentsize,
        bad_phnum,
        bad_ph_range
    };

    struct ElfHeader64 {
        util::u8 ident[16]{};
        util::u16 type{0};
        util::u16 machine{0};
        util::u32 version{0};
        util::u64 entry{0};
        util::u64 phoff{0};
        util::u64 shoff{0};
        util::u32 flags{0};
        util::u16 ehsize{0};
        util::u16 phentsize{0};
        util::u16 phnum{0};
        util::u16 shentsize{0};
        util::u16 shnum{0};
        util::u16 shstrndx{0};
    };

    struct ElfProgramHeader64 {
        util::u32 type{0};
        util::u32 flags{0};
        util::u64 offset{0};
        util::u64 vaddr{0};
        util::u64 paddr{0};
        util::u64 filesz{0};
        util::u64 memsz{0};
        util::u64 align{0};
    };

    constexpr util::u32 kElfPtLoad = 1;
    constexpr util::u32 kElfPfX = 0x1;
    constexpr util::u32 kElfPfW = 0x2;
    constexpr util::u32 kElfPfR = 0x4;

    inline ElfErrc validate_elf_header(const void* base, util::usize size) noexcept {
        if (!base || size < sizeof(ElfHeader64)) return ElfErrc::bad_header;
        const auto* hdr = static_cast<const ElfHeader64*>(base);
        if (hdr->ident[0] != 0x7f || hdr->ident[1] != 'E' ||
            hdr->ident[2] != 'L' || hdr->ident[3] != 'F') {
            return ElfErrc::bad_magic;
        }
        if (hdr->ident[4] != 2) return ElfErrc::bad_class;  // 64-bit only (v0)
        if (hdr->ident[5] != 1) return ElfErrc::bad_endian; // little-endian only (v0)
        if (hdr->phoff > size) return ElfErrc::bad_phoff;
        if (hdr->phentsize != 0 && hdr->phentsize < sizeof(ElfProgramHeader64)) {
            return ElfErrc::bad_phentsize;
        }
        if (hdr->phnum != 0 && hdr->phentsize == 0) return ElfErrc::bad_phnum;
        if (hdr->phnum != 0) {
            const auto ph_bytes = static_cast<util::u64>(hdr->phnum) * hdr->phentsize;
            const auto end = static_cast<util::u64>(hdr->phoff) + ph_bytes;
            if (end > size) return ElfErrc::bad_ph_range;
        }
        return ElfErrc::ok;
    }

    struct ElfLoadConfig {
        const void* image_base{nullptr};
        util::usize image_size{0};
        void* load_base{nullptr};
        util::usize load_size{0};
        util::usize load_align{16};
    };

    inline util::Result<ProgramImage> load_elf_image(const ElfLoadConfig& cfg) noexcept {
        if (!cfg.image_base || cfg.image_size == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto header_ok = validate_elf_header(cfg.image_base, cfg.image_size);
        if (header_ok != ElfErrc::ok) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (!cfg.load_base) {
            return util::unexpected(util::Errc::not_supported);
        }
        if (cfg.load_size == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (cfg.load_align != 0) {
            const auto addr = modulex::to_addr(cfg.load_base);
            if ((addr % cfg.load_align) != 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
        }
        const auto* hdr = static_cast<const ElfHeader64*>(cfg.image_base);
        if (hdr->entry == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (hdr->phoff != 0 && hdr->phentsize == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (hdr->phnum != 0) {
            const auto* base = static_cast<const util::u8*>(cfg.image_base);
            const auto* ph = reinterpret_cast<const ElfProgramHeader64*>(base + hdr->phoff);
            bool has_load = false;
            util::u64 min_vaddr = 0;
            // Policy: map the lowest PT_LOAD vaddr to load_base, entry is offset from that base.
            for (util::u16 i = 0; i < hdr->phnum; ++i) {
                if (ph[i].type == kElfPtLoad) {
                    has_load = true;
                    if (min_vaddr == 0 || ph[i].vaddr < min_vaddr) {
                        min_vaddr = ph[i].vaddr;
                    }
                }
                if (ph[i].offset + ph[i].filesz > cfg.image_size) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                if (ph[i].memsz < ph[i].filesz) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                if ((ph[i].flags & kElfPfW) && (ph[i].flags & kElfPfX)) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                if (ph[i].align != 0 && (ph[i].vaddr % ph[i].align) != 0) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                if (ph[i].align != 0 &&
                    (ph[i].offset % ph[i].align) != (ph[i].vaddr % ph[i].align)) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
            }
            if (!has_load) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (hdr->entry < min_vaddr) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            bool entry_ok = false;
            for (util::u16 i = 0; i < hdr->phnum; ++i) {
                if (ph[i].type != kElfPtLoad) continue;
                const auto dst_off = ph[i].vaddr - min_vaddr;
                const auto end_off = dst_off + ph[i].memsz;
                if (end_off > cfg.load_size) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                for (util::u16 j = 0; j < i; ++j) {
                    if (ph[j].type != kElfPtLoad) continue;
                    const auto prev_off = ph[j].vaddr - min_vaddr;
                    const auto prev_end = prev_off + ph[j].memsz;
                    if (dst_off < prev_end && prev_off < end_off) {
                        return util::unexpected(util::Errc::invalid_arg);
                    }
                }
                if (hdr->entry >= ph[i].vaddr && hdr->entry < (ph[i].vaddr + ph[i].memsz)) {
                    entry_ok = true;
                }
                auto* dst = static_cast<util::u8*>(cfg.load_base) + dst_off;
                const auto* src = base + ph[i].offset;
                if (ph[i].filesz > 0) {
                    std::memcpy(dst, src, static_cast<std::size_t>(ph[i].filesz));
                }
                if (ph[i].memsz > ph[i].filesz) {
                    std::memset(dst + ph[i].filesz, 0,
                        static_cast<std::size_t>(ph[i].memsz - ph[i].filesz));
                }
            }
            if (!entry_ok) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            ProgramImage image{};
            image.kind = ImageKind::elf;
            image.name = {};
            const auto entry_off = static_cast<util::u64>(hdr->entry - min_vaddr);
            image.entry = addr_to_entry(
                modulex::to_addr(cfg.load_base) + static_cast<modulex::Addr>(entry_off));
            if (!image.entry) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return image;
        }
        return util::unexpected(util::Errc::invalid_arg);
    }
}

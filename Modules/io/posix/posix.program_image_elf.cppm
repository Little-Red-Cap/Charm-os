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

    struct ElfHeader32 {
        util::u8 ident[16]{};
        util::u16 type{0};
        util::u16 machine{0};
        util::u32 version{0};
        util::u32 entry{0};
        util::u32 phoff{0};
        util::u32 shoff{0};
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

    struct ElfProgramHeader32 {
        util::u32 type{0};
        util::u32 offset{0};
        util::u32 vaddr{0};
        util::u32 paddr{0};
        util::u32 filesz{0};
        util::u32 memsz{0};
        util::u32 flags{0};
        util::u32 align{0};
    };

    constexpr util::u32 kElfPtLoad = 1;
    constexpr util::u32 kElfPfX = 0x1;
    constexpr util::u32 kElfPfW = 0x2;
    constexpr util::u32 kElfPfR = 0x4;

    inline ElfErrc validate_elf_header(const void* base, util::usize size) noexcept {
        if (!base || size < 16) return ElfErrc::bad_header;
        const auto* ident = static_cast<const util::u8*>(base);
        if (ident[0] != 0x7f || ident[1] != 'E' ||
            ident[2] != 'L' || ident[3] != 'F') {
            return ElfErrc::bad_magic;
        }
        const auto klass = ident[4];
        if (klass != 1 && klass != 2) return ElfErrc::bad_class;
        if (ident[5] != 1) return ElfErrc::bad_endian; // little-endian only (v0)
        if (klass == 1) {
            if (size < sizeof(ElfHeader32)) return ElfErrc::bad_header;
            const auto* hdr = static_cast<const ElfHeader32*>(base);
            if (hdr->phoff > size) return ElfErrc::bad_phoff;
            if (hdr->phentsize != 0 && hdr->phentsize < sizeof(ElfProgramHeader32)) {
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
        if (size < sizeof(ElfHeader64)) return ElfErrc::bad_header;
        const auto* hdr = static_cast<const ElfHeader64*>(base);
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

    template <class Header, class Phdr>
    util::Result<ProgramImage> load_elf_image_impl(const Header* hdr,
                                                   const util::u8* base,
                                                   const ElfLoadConfig& cfg) noexcept {
        if (hdr->entry == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (hdr->phoff != 0 && hdr->phentsize == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (hdr->phnum == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto ph_base = base + static_cast<util::usize>(hdr->phoff);
        const auto phentsize = static_cast<util::usize>(hdr->phentsize);
        auto ph_at = [&](util::u16 index) noexcept -> const Phdr* {
            return reinterpret_cast<const Phdr*>(ph_base + (phentsize * index));
        };
        bool has_load = false;
        bool min_vaddr_set = false;
        util::u64 min_vaddr = 0;
        for (util::u16 i = 0; i < hdr->phnum; ++i) {
            const auto* ph = ph_at(i);
            if (ph->type != kElfPtLoad) continue;
            const auto memsz = static_cast<util::u64>(ph->memsz);
            const auto filesz = static_cast<util::u64>(ph->filesz);
            if (memsz == 0 && filesz == 0) {
                continue;
            }
            has_load = true;
            const auto vaddr = static_cast<util::u64>(ph->vaddr);
            if (!min_vaddr_set || vaddr < min_vaddr) {
                min_vaddr = vaddr;
                min_vaddr_set = true;
            }
            const auto offset = static_cast<util::u64>(ph->offset);
            if (offset + filesz > cfg.image_size) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (memsz < filesz) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if ((ph->flags & kElfPfW) && (ph->flags & kElfPfX)) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            const auto align = static_cast<util::u64>(ph->align);
            if (align != 0 && (offset % align) != (vaddr % align)) {
                return util::unexpected(util::Errc::invalid_arg);
            }
        }
        if (!has_load) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        const auto entry = static_cast<util::u64>(hdr->entry);
        if (entry < min_vaddr) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        bool entry_ok = false;
        for (util::u16 i = 0; i < hdr->phnum; ++i) {
            const auto* ph = ph_at(i);
            if (ph->type != kElfPtLoad) continue;
            const auto memsz = static_cast<util::u64>(ph->memsz);
            const auto filesz = static_cast<util::u64>(ph->filesz);
            if (memsz == 0 && filesz == 0) continue;
            const auto vaddr = static_cast<util::u64>(ph->vaddr);
            const auto offset = static_cast<util::u64>(ph->offset);
            const auto dst_off = vaddr - min_vaddr;
            const auto end_off = dst_off + memsz;
            if (end_off > cfg.load_size) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::u16 j = 0; j < i; ++j) {
                const auto* prev = ph_at(j);
                if (prev->type != kElfPtLoad) continue;
                const auto prev_memsz = static_cast<util::u64>(prev->memsz);
                const auto prev_filesz = static_cast<util::u64>(prev->filesz);
                if (prev_memsz == 0 && prev_filesz == 0) continue;
                const auto prev_off = static_cast<util::u64>(prev->vaddr) - min_vaddr;
                const auto prev_end = prev_off + prev_memsz;
                if (dst_off < prev_end && prev_off < end_off) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
            }
            if (entry >= vaddr && entry < (vaddr + memsz)) {
                entry_ok = true;
            }
            auto* dst = static_cast<util::u8*>(cfg.load_base) + dst_off;
            const auto* src = base + offset;
            if (filesz > 0) {
                std::memcpy(dst, src, static_cast<std::size_t>(filesz));
            }
            if (memsz > filesz) {
                std::memset(dst + filesz, 0, static_cast<std::size_t>(memsz - filesz));
            }
        }
        if (!entry_ok) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        ProgramImage image{};
        image.kind = ImageKind::elf;
        image.name = {};
        image.entry_abi = ImageEntryAbi::main_argv_envp_v1;
        const auto entry_off = static_cast<util::u64>(entry - min_vaddr);
        image.entry = addr_to_entry(
            modulex::to_addr(cfg.load_base) + static_cast<modulex::Addr>(entry_off));
        if (!image.entry) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        return image;
    }

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
        const auto* base = static_cast<const util::u8*>(cfg.image_base);
        const auto* ident = base;
        if (ident[4] == 1) {
            const auto* hdr32 = reinterpret_cast<const ElfHeader32*>(cfg.image_base);
            return load_elf_image_impl<ElfHeader32, ElfProgramHeader32>(hdr32, base, cfg);
        }
        const auto* hdr64 = reinterpret_cast<const ElfHeader64*>(cfg.image_base);
        return load_elf_image_impl<ElfHeader64, ElfProgramHeader64>(hdr64, base, cfg);
    }
}

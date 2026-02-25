module;

#include <cstddef>
#include <cstdint>

export module module_view;

import module_core;
import util.core;

export namespace modulex {
    enum class ImageError : util::u8 {
        ok = 0,
        null_image,
        bad_magic,
        bad_version,
        bad_size,
        bad_entry,
        bad_reloc,
        bad_sym,
        bad_dep
    };

    struct ImageStatus {
        bool ok{false};
        ImageError err{ImageError::ok};
    };

    struct ImageView {
        const ImageHeader* header{nullptr};
        const std::byte* base{nullptr};
        util::u32 size{0};
    };

    struct SegmentBases {
        Addr image_base{0};
        Addr text_base{0};
        Addr ro_base{0};
        Addr data_base{0};
    };

    inline ImageView make_view(const void* data, util::u32 size) noexcept {
        if (!data || size < sizeof(ImageHeader)) return {};
        return ImageView{
            reinterpret_cast<const ImageHeader*>(data),
            reinterpret_cast<const std::byte*>(data),
            size
        };
    }

    inline util::u32 layout_text(const ImageHeader& h) noexcept {
        return h.text_offset != 0 ? h.text_offset : static_cast<util::u32>(sizeof(ImageHeader));
    }

    inline util::u32 layout_ro(const ImageHeader& h) noexcept {
        const auto text = layout_text(h);
        return h.ro_offset != 0 ? h.ro_offset : static_cast<util::u32>(text + h.text_size);
    }

    inline util::u32 layout_data(const ImageHeader& h) noexcept {
        const auto ro = layout_ro(h);
        return h.data_offset != 0 ? h.data_offset : static_cast<util::u32>(ro + h.ro_size);
    }

    inline util::u32 layout_rel(const ImageHeader& h) noexcept {
        const auto data = layout_data(h);
        return h.rel_offset != 0 ? h.rel_offset : static_cast<util::u32>(data + h.data_size);
    }

    inline util::u32 layout_sym(const ImageHeader& h) noexcept {
        const auto rel = layout_rel(h);
        return h.sym_offset != 0 ? h.sym_offset : static_cast<util::u32>(rel + h.rel_size);
    }

    inline util::u32 layout_str(const ImageHeader& h) noexcept {
        const auto sym = layout_sym(h);
        return h.str_offset != 0 ? h.str_offset : static_cast<util::u32>(sym + h.sym_size);
    }

    inline util::u32 layout_dep(const ImageHeader& h) noexcept {
        const auto str = layout_str(h);
        return h.dep_offset != 0 ? h.dep_offset : static_cast<util::u32>(str + h.str_size);
    }

    inline SegmentBases default_bases(const ImageView& view) noexcept {
        if (!view.header || !view.base) return {};
        const auto base = to_addr(view.base);
        const auto& h = *view.header;
        return SegmentBases{
            base,
            base + layout_text(h),
            base + layout_ro(h),
            base + layout_data(h)
        };
    }

    inline bool is_xip_text(const ImageHeader& h) noexcept {
        return (h.flags & static_cast<util::u16>(ImageFlags::xip_text)) != 0;
    }

    inline bool entry_in_text(const ImageView& view, Addr entry) noexcept {
        if (!view.header || !view.base) return false;
        const auto bases = default_bases(view);
        const auto start = bases.text_base;
        const auto end = bases.text_base + view.header->text_size;
        return entry >= start && entry < end;
    }

    inline bool can_exec_internal(const ImageView& view, Addr entry) noexcept {
        if (!view.header) return false;
        if (!is_xip_text(*view.header)) return false;
        return entry_in_text(view, entry);
    }

    inline Addr resolve_target(const ImageHeader& h, const SegmentBases& bases, util::u32 offset) noexcept {
        const auto text = layout_text(h);
        const auto ro = layout_ro(h);
        const auto data = layout_data(h);
        if (offset >= text && offset < text + h.text_size) {
            return bases.text_base + (offset - text);
        }
        if (offset >= ro && offset < ro + h.ro_size) {
            return bases.ro_base + (offset - ro);
        }
        if (offset >= data && offset < data + h.data_size) {
            return bases.data_base + (offset - data);
        }
        return bases.image_base + offset;
    }

    inline bool section_fits(util::u32 off, util::u32 size, util::u32 total) noexcept {
        if (off > total) return false;
        if (size > total) return false;
        return (off + size) <= total;
    }

    inline ImageStatus validate(const ImageView& view) noexcept {
        if (!view.header || !view.base) return {false, ImageError::null_image};
        if (view.size < sizeof(ImageHeader)) return {false, ImageError::bad_size};
        const auto& h = *view.header;
        if (h.magic != k_magic) return {false, ImageError::bad_magic};
        if (h.version != k_version) return {false, ImageError::bad_version};
        if (h.text_size == 0) return {false, ImageError::bad_size};
        if (h.entry_offset >= h.text_size) return {false, ImageError::bad_entry};
        if ((h.rel_size % sizeof(Reloc)) != 0) return {false, ImageError::bad_reloc};
        if ((h.sym_size % sizeof(Symbol)) != 0) return {false, ImageError::bad_sym};
        if ((h.dep_size % sizeof(Dependency)) != 0) return {false, ImageError::bad_dep};

        util::u32 total = view.size;
        if (h.image_size != 0) {
            if (h.image_size > view.size || h.image_size < sizeof(ImageHeader)) {
                return {false, ImageError::bad_size};
            }
            total = h.image_size;
        }

        const auto text = layout_text(h);
        const auto ro = layout_ro(h);
        const auto data = layout_data(h);
        const auto rel = layout_rel(h);
        const auto sym = layout_sym(h);
        const auto str = layout_str(h);
        const auto dep = layout_dep(h);
        if (!section_fits(text, h.text_size, total)) return {false, ImageError::bad_size};
        if (!section_fits(ro, h.ro_size, total)) return {false, ImageError::bad_size};
        if (!section_fits(data, h.data_size, total)) return {false, ImageError::bad_size};
        if (!section_fits(rel, h.rel_size, total)) return {false, ImageError::bad_reloc};
        if (!section_fits(sym, h.sym_size, total)) return {false, ImageError::bad_sym};
        if (!section_fits(str, h.str_size, total)) return {false, ImageError::bad_size};
        if (!section_fits(dep, h.dep_size, total)) return {false, ImageError::bad_dep};

        if (h.rel_size != 0) {
            const auto rel_base = to_addr(view.base) + rel;
            const auto rel_count = h.rel_size / sizeof(Reloc);
            const auto sym_count = h.sym_size / sizeof(Symbol);
            const auto rels = reinterpret_cast<const Reloc*>(rel_base);
            for (util::u32 i = 0; i < rel_count; ++i) {
                const auto& r = rels[i];
                if (r.offset + sizeof(util::u32) > total) return {false, ImageError::bad_reloc};
                if (r.sym_index != 0xFFFFFFFFu && r.sym_index >= sym_count) {
                    return {false, ImageError::bad_sym};
                }
            }
        }
        return {true, ImageError::ok};
    }
}

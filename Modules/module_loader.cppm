module;

#include <cstddef>
#include <cstdint>
#include <string_view>

export module module_loader;

import module_core;
import module_view;
import util.core;

export namespace modulex {
    struct SymView {
        const Symbol* sym{nullptr};
        std::string_view name{};
    };

    struct SymReader {
        const ImageHeader* img{nullptr};
        const char* strtab{nullptr};
        util::u32 str_size{0};

        SymView at(const Symbol& s) const noexcept {
            if (!strtab || s.name_offset >= str_size) return {&s, {}};
            const char* base = strtab + s.name_offset;
            util::u32 len = 0;
            while (s.name_offset + len < str_size && base[len] != '\0') ++len;
            return {&s, std::string_view{base, len}};
        }
    };

    struct LoadResult {
        bool ok{false};
        util::usize entry{0};
        const Symbol* symtab{nullptr};
        util::u32 sym_count{0};
        SymReader sym_reader{};
        const Dependency* deps{nullptr};
        util::u32 dep_count{0};
    };

    struct Loader {
        static LoadResult load(const ImageHeader* img) noexcept {
            if (!img || img->magic != k_magic || img->version != k_version) {
                return {false, 0};
            }
            if (img->text_size == 0) return {false, 0};
            if (img->entry_offset >= img->text_size) return {false, 0};
            const auto base = reinterpret_cast<util::usize>(img);
            const auto text_base = base + (img->text_offset != 0 ? img->text_offset : sizeof(ImageHeader));
            const auto entry = text_base + img->entry_offset;
            const auto sym_base = (img->sym_offset != 0)
                ? (base + img->sym_offset)
                : (base + sizeof(ImageHeader) + img->text_size + img->ro_size + img->data_size + img->rel_size);
            const auto sym_count = static_cast<util::u32>(img->sym_size / sizeof(Symbol));
            SymReader reader{};
            reader.img = img;
            const auto str_base = (img->str_offset != 0) ? (base + img->str_offset) : (sym_base + img->sym_size);
            reader.strtab = reinterpret_cast<const char*>(str_base);
            reader.str_size = img->str_size;
            const auto dep_base = (img->dep_offset != 0)
                ? (base + img->dep_offset)
                : (str_base + img->str_size);
            const auto dep_count = static_cast<util::u32>(img->dep_size / sizeof(Dependency));
            return {true, entry, reinterpret_cast<const Symbol*>(sym_base), sym_count, reader,
                    reinterpret_cast<const Dependency*>(dep_base), dep_count};
        }

        static LoadResult load(const ImageView& view) noexcept {
            const auto bases = default_bases(view);
            return load(view, bases);
        }

        static LoadResult load(const ImageView& view, const SegmentBases& bases) noexcept {
            const auto status = validate(view);
            if (!status.ok) {
                return {false, 0};
            }
            const auto& img = *view.header;
            const auto base = reinterpret_cast<util::usize>(view.base);
            const auto entry = bases.text_base + img.entry_offset;
            const auto sym_base = base + layout_sym(img);
            const auto sym_count = static_cast<util::u32>(img.sym_size / sizeof(Symbol));
            SymReader reader{};
            reader.img = &img;
            const auto str_base = base + layout_str(img);
            reader.strtab = reinterpret_cast<const char*>(str_base);
            reader.str_size = img.str_size;
            const auto dep_base = base + layout_dep(img);
            const auto dep_count = static_cast<util::u32>(img.dep_size / sizeof(Dependency));
            return {true, entry, reinterpret_cast<const Symbol*>(sym_base), sym_count, reader,
                    reinterpret_cast<const Dependency*>(dep_base), dep_count};
        }
    };
}

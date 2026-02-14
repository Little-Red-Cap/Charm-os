module;

#include <cstddef>
#include <cstdint>
#include <string_view>

export module module_link;

import module_core;
import util.core;

export namespace modulex {
    using ResolveExternal = bool (*)(std::string_view name, util::usize& out_addr) noexcept;
    using ResolveDependency = bool (*)(std::string_view name, std::string_view version) noexcept;
    using ResolveDependencyCtx = bool (*)(void* ctx, std::string_view name, std::string_view version) noexcept;

    enum class DepError : util::u8 {
        ok = 0,
        bad_name,
        resolve_failed
    };

    struct DepStatus {
        bool ok{false};
        util::u32 failed_index{0};
        DepError error{DepError::ok};
    };

    struct Linker {
        static bool bind_externals(const ImageHeader* img, util::usize base_addr, ResolveExternal resolver) noexcept {
            if (!img || !resolver) return false;
            if (img->sym_size == 0) return true;
            const auto sym_base = base_addr + (img->sym_offset != 0
                ? img->sym_offset
                : (sizeof(ImageHeader) + img->text_size + img->ro_size + img->data_size + img->rel_size));
            const auto sym_count = img->sym_size / sizeof(Symbol);
            const auto str_base = base_addr + (img->str_offset != 0 ? img->str_offset : (sym_base + img->sym_size));
            const auto str_size = img->str_size;
            auto* syms = reinterpret_cast<Symbol*>(sym_base);
            for (util::u32 i = 0; i < sym_count; ++i) {
                auto& s = syms[i];
                if (s.kind != SymbolKind::external) continue;
                if (s.name_offset >= str_size) return false;
                const char* base = reinterpret_cast<const char*>(str_base + s.name_offset);
                util::u32 len = 0;
                while (s.name_offset + len < str_size && base[len] != '\0') ++len;
                util::usize addr = 0;
                if (!resolver(std::string_view{base, len}, addr)) return false;
                s.value = addr;
            }
            return true;
        }

        static bool validate_deps(const ImageHeader* img, util::usize base_addr, ResolveDependency resolver) noexcept {
            if (!img || !resolver) return false;
            if (img->dep_size == 0) return true;
            const auto dep_base = base_addr + (img->dep_offset != 0
                ? img->dep_offset
                : (sizeof(ImageHeader) + img->text_size + img->ro_size + img->data_size + img->rel_size +
                   img->sym_size + img->str_size));
            const auto dep_count = img->dep_size / sizeof(Dependency);
            const auto str_base = base_addr + (img->str_offset != 0 ? img->str_offset : (dep_base + img->dep_size));
            const auto str_size = img->str_size;
            auto* deps = reinterpret_cast<const Dependency*>(dep_base);
            for (util::u32 i = 0; i < dep_count; ++i) {
                const auto& d = deps[i];
                if (d.name_offset >= str_size) return false;
                const char* base = reinterpret_cast<const char*>(str_base + d.name_offset);
                util::u32 len = 0;
                while (d.name_offset + len < str_size && base[len] != '\0') ++len;
                std::string_view name{base, len};
                std::string_view version{};
                if (d.version_offset < str_size) {
                    const char* vbase = reinterpret_cast<const char*>(str_base + d.version_offset);
                    util::u32 vlen = 0;
                    while (d.version_offset + vlen < str_size && vbase[vlen] != '\0') ++vlen;
                    version = std::string_view{vbase, vlen};
                }
                if (!resolver(name, version)) return false;
            }
            return true;
        }

        static DepStatus validate_deps_ctx(const ImageHeader* img, util::usize base_addr,
                                      ResolveDependencyCtx resolver, void* ctx) noexcept {
            if (!img || !resolver) return {false, 0, DepError::bad_name};
            if (img->dep_size == 0) return {true, 0, DepError::ok};
            const auto dep_base = base_addr + (img->dep_offset != 0
                ? img->dep_offset
                : (sizeof(ImageHeader) + img->text_size + img->ro_size + img->data_size + img->rel_size +
                   img->sym_size + img->str_size));
            const auto dep_count = img->dep_size / sizeof(Dependency);
            const auto str_base = base_addr + (img->str_offset != 0 ? img->str_offset : (dep_base + img->dep_size));
            const auto str_size = img->str_size;
            auto* deps = reinterpret_cast<const Dependency*>(dep_base);
            for (util::u32 i = 0; i < dep_count; ++i) {
                const auto& d = deps[i];
                if (d.name_offset >= str_size) return {false, i, DepError::bad_name};
                const char* base = reinterpret_cast<const char*>(str_base + d.name_offset);
                util::u32 len = 0;
                while (d.name_offset + len < str_size && base[len] != '\0') ++len;
                std::string_view name{base, len};
                std::string_view version{};
                if (d.version_offset < str_size) {
                    const char* vbase = reinterpret_cast<const char*>(str_base + d.version_offset);
                    util::u32 vlen = 0;
                    while (d.version_offset + vlen < str_size && vbase[vlen] != '\0') ++vlen;
                    version = std::string_view{vbase, vlen};
                }
                if (!resolver(ctx, name, version)) return {false, i, DepError::resolve_failed};
            }
            return {true, 0, DepError::ok};
        }

        static bool relocate(const ImageHeader* img, util::usize base_addr) noexcept {
            if (!img || img->magic != k_magic || img->version != k_version) return false;
            if (img->rel_size == 0) return true;
            if (img->rel_size % sizeof(Reloc) != 0) return false;
            const auto rel_base = base_addr + (img->rel_offset != 0
                ? img->rel_offset
                : (sizeof(ImageHeader) + img->text_size + img->ro_size + img->data_size));
            const auto count = img->rel_size / sizeof(Reloc);
            auto rels = reinterpret_cast<const Reloc*>(rel_base);
            const auto sym_base = base_addr + (img->sym_offset != 0
                ? img->sym_offset
                : (sizeof(ImageHeader) + img->text_size + img->ro_size + img->data_size + img->rel_size));
            auto* syms = reinterpret_cast<const Symbol*>(sym_base);
            const auto sym_count = img->sym_size / sizeof(Symbol);

            for (util::u32 i = 0; i < count; ++i) {
                const auto& r = rels[i];
                const auto target = base_addr + r.offset;
                util::usize sym_addr = 0;
                if (r.sym_index != 0xFFFFFFFFu && r.sym_index < sym_count) {
                    const auto& s = syms[r.sym_index];
                    if (s.kind == SymbolKind::external) {
                        sym_addr = static_cast<util::usize>(s.value);
                    } else {
                        const auto text_base = base_addr + (img->text_offset != 0
                            ? img->text_offset
                            : sizeof(ImageHeader));
                        sym_addr = text_base + s.value;
                    }
                } else {
                    sym_addr = base_addr;
                }
                switch (r.type) {
                case RelocType::abs_addr: {
                    auto* slot = reinterpret_cast<util::usize*>(target);
                    *slot = sym_addr + r.addend;
                    break;
                }
                case RelocType::rel32: {
                    auto* slot = reinterpret_cast<util::u32*>(target);
                    const auto value = static_cast<util::u32>(
                        (sym_addr + r.addend) - (target + sizeof(util::u32)));
                    *slot = value;
                    break;
                }
                case RelocType::none:
                default:
                    break;
                }
            }
            return true;
        }
    };
}

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <array>
#include <cstring>
#include <string_view>

import charm.core;
import module_core;
import module_link;
import module_loader;
import module_registry;

static void demo_entry() {
    std::printf("[module_demo] entry invoked\n");
}

static void external_func() {
    std::printf("[module_demo] external invoked\n");
}

static bool resolve_symbol(std::string_view name, util::usize& out) noexcept {
    if (name == "ext_fn") {
        out = reinterpret_cast<util::usize>(&external_func);
        return true;
    }
    return false;
}

static bool resolve_dep_ctx(void* ctx, std::string_view name, std::string_view version) noexcept {
    auto* reg = static_cast<modulex::Registry<4>*>(ctx);
    if (!reg) return false;
    auto* info = reg->find(name);
    if (!info) return false;
    return modulex::match_version(version, info->version);
}

int main() {
    struct DemoImage {
        modulex::ImageHeader hdr{};
        std::array<std::byte, 16> text{};
        util::usize data_entry{0};
        util::usize data_ext{0};
        util::u32 rel32_slot{0};
        modulex::Reloc rels[3]{};
        modulex::Symbol syms[2]{};
        char strtab[24]{};
        modulex::Dependency deps[1]{};
    };

    DemoImage img{};
    img.hdr.magic = modulex::k_magic;
    img.hdr.version = modulex::k_version;
    img.hdr.text_offset = static_cast<util::u32>(offsetof(DemoImage, text));
    img.hdr.data_offset = static_cast<util::u32>(offsetof(DemoImage, data_entry));
    img.hdr.rel_offset = static_cast<util::u32>(offsetof(DemoImage, rels));
    img.hdr.sym_offset = static_cast<util::u32>(offsetof(DemoImage, syms));
    img.hdr.text_size = static_cast<util::u32>(img.text.size());
    img.hdr.data_size = static_cast<util::u32>(sizeof(img.data_entry) + sizeof(img.data_ext) + sizeof(img.rel32_slot));
    img.hdr.rel_size = static_cast<util::u32>(sizeof(img.rels));
    img.hdr.sym_size = static_cast<util::u32>(sizeof(img.syms));
    img.hdr.entry_offset = 0;
    img.hdr.str_offset = static_cast<util::u32>(offsetof(DemoImage, strtab));
    img.hdr.str_size = static_cast<util::u32>(sizeof(img.strtab));
    img.hdr.dep_offset = static_cast<util::u32>(offsetof(DemoImage, deps));
    img.hdr.dep_size = static_cast<util::u32>(sizeof(img.deps));
    img.hdr.image_size = static_cast<util::u32>(sizeof(DemoImage));

    img.rels[0].offset = static_cast<util::u32>(offsetof(DemoImage, data_entry));
    img.rels[0].type = modulex::RelocType::abs_addr;
    img.rels[0].sym_index = 0;
    img.rels[0].addend = 0;

    img.rels[1].offset = static_cast<util::u32>(offsetof(DemoImage, data_ext));
    img.rels[1].type = modulex::RelocType::abs_addr;
    img.rels[1].sym_index = 1;
    img.rels[1].addend = 0;

    img.rels[2].offset = static_cast<util::u32>(offsetof(DemoImage, rel32_slot));
    img.rels[2].type = modulex::RelocType::rel32;
    img.rels[2].sym_index = 0;
    img.rels[2].addend = 0;

    img.syms[0].name_offset = 0;
    img.syms[0].value = static_cast<util::usize>(img.hdr.entry_offset);
    img.syms[0].size = 0;
    img.syms[0].kind = modulex::SymbolKind::global;
    img.syms[0].flags = 0;

    img.syms[1].name_offset = 6;
    img.syms[1].value = 0;
    img.syms[1].size = 0;
    img.syms[1].kind = modulex::SymbolKind::external;
    img.syms[1].flags = 0;

    std::memcpy(img.strtab, "entry\0ext_fn\0host\0^1\0", 22);
    img.deps[0].name_offset = 13;
    img.deps[0].version_offset = 18;

    modulex::Registry<4> reg{};
    (void)reg.add(modulex::ModuleInfo{"host", "v1", 0});

    auto loaded = modulex::Loader::load(&img.hdr);
    const auto base = reinterpret_cast<util::usize>(&img);
    auto dep_status = modulex::Linker::validate_deps_ctx(&img.hdr, base, &resolve_dep_ctx, &reg);
    (void)modulex::Linker::bind_externals(&img.hdr, base, &resolve_symbol);
    auto linked = modulex::Linker::relocate(&img.hdr, base);
    std::printf("[module_demo] load=%d deps=%d dep_fail=%u dep_err=%u link=%d entry=0x%zx\n",
                loaded.ok ? 1 : 0, dep_status.ok ? 1 : 0, dep_status.failed_index,
                static_cast<unsigned>(dep_status.error),
                linked ? 1 : 0, static_cast<std::size_t>(loaded.entry));
    std::printf("[module_demo] reloc entry=0x%zx expected=0x%zx\n",
                static_cast<std::size_t>(img.data_entry),
                static_cast<std::size_t>(reinterpret_cast<util::usize>(&img) + img.hdr.text_offset));
    std::printf("[module_demo] reloc ext=0x%zx expected=0x%zx\n",
                static_cast<std::size_t>(img.data_ext),
                static_cast<std::size_t>(reinterpret_cast<util::usize>(&external_func)));
    {
        const auto slot = reinterpret_cast<util::usize>(&img.rel32_slot);
        const auto target = reinterpret_cast<util::usize>(&img) + img.hdr.text_offset;
        const auto expected = static_cast<util::u32>(target - (slot + sizeof(util::u32)));
        std::printf("[module_demo] rel32 slot=0x%x expected=0x%x\n", img.rel32_slot, expected);
    }
    if (loaded.symtab && loaded.sym_count > 1) {
        auto v0 = loaded.sym_reader.at(loaded.symtab[0]);
        auto v1 = loaded.sym_reader.at(loaded.symtab[1]);
        std::printf("[module_demo] sym0 name=%.*s value=0x%zx kind=%u\n",
                    static_cast<int>(v0.name.size()), v0.name.data(),
                    static_cast<std::size_t>(loaded.symtab[0].value),
                    static_cast<unsigned>(loaded.symtab[0].kind));
        std::printf("[module_demo] sym1 name=%.*s value=0x%zx kind=%u\n",
                    static_cast<int>(v1.name.size()), v1.name.data(),
                    static_cast<std::size_t>(loaded.symtab[1].value),
                    static_cast<unsigned>(loaded.symtab[1].kind));
    }
    if (loaded.deps && loaded.dep_count > 0) {
        auto dep = loaded.deps[0];
        auto dv = loaded.sym_reader.at(modulex::Symbol{dep.name_offset});
        auto vv = loaded.sym_reader.at(modulex::Symbol{dep.version_offset});
        std::printf("[module_demo] dep name=%.*s version=%.*s\n",
                    static_cast<int>(dv.name.size()), dv.name.data(),
                    static_cast<int>(vv.name.size()), vv.name.data());
    }

    if (loaded.ok) demo_entry();
    return 0;
}

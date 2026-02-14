module;

#include <cstddef>
#include <cstdint>

export module module_core;

import util.core;

export namespace modulex {
    constexpr util::u32 k_magic = 0x43484D4D; // 'CHMM'
    constexpr util::u16 k_version = 1;

    enum class RelocType : util::u32 {
        none = 0,
        abs_addr = 1,
        rel32 = 2
    };

    struct ImageHeader {
        util::u32 magic{k_magic}; // 'CHMM'
        util::u16 version{k_version};
        util::u16 flags{0};
        util::u32 entry_offset{0};
        util::u32 text_offset{0};
        util::u32 ro_offset{0};
        util::u32 data_offset{0};
        util::u32 rel_offset{0};
        util::u32 sym_offset{0};
        util::u32 str_offset{0};
        util::u32 str_size{0};
        util::u32 dep_offset{0};
        util::u32 dep_size{0};
        util::u32 text_size{0};
        util::u32 ro_size{0};
        util::u32 data_size{0};
        util::u32 bss_size{0};
        util::u32 rel_size{0};
        util::u32 sym_size{0};
    };

    struct Reloc {
        util::u32 offset{0};
        RelocType type{RelocType::none};
        util::u32 sym_index{0xFFFFFFFFu};
        util::u32 addend{0};
    };

    enum class SymbolKind : util::u16 {
        local = 0,
        global = 1,
        external = 2
    };

    struct Symbol {
        util::u32 name_offset{0};
        util::usize value{0};
        util::u32 size{0};
        SymbolKind kind{SymbolKind::local};
        util::u16 flags{0};
    };

    struct Dependency {
        util::u32 name_offset{0};
        util::u32 version_offset{0};
    };
}

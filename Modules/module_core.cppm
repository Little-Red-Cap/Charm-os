module;

#include <cstddef>
#include <cstdint>

export module module_core;

import util.core;

export namespace modulex {
    struct ImageHeader {
        util::u32 magic{0x43484D4D}; // 'CHMM'
        util::u16 version{1};
        util::u16 flags{0};
        util::u32 entry_offset{0};
        util::u32 text_size{0};
        util::u32 ro_size{0};
        util::u32 data_size{0};
        util::u32 bss_size{0};
        util::u32 rel_size{0};
        util::u32 sym_size{0};
    };

    struct Reloc {
        util::u32 offset{0};
        util::u32 type{0};
        util::u32 addend{0};
    };

    struct Symbol {
        util::u32 name_offset{0};
        util::u32 value{0};
        util::u32 size{0};
        util::u16 kind{0};
        util::u16 flags{0};
    };
}

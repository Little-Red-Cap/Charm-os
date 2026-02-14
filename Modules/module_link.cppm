module;

#include <cstddef>
#include <cstdint>

export module module_link;

import module_core;
import util.core;

export namespace modulex {
    struct Linker {
        static bool relocate(const ImageHeader* img, util::u32 base_addr) noexcept {
            (void)img;
            (void)base_addr;
            return false;
        }
    };
}

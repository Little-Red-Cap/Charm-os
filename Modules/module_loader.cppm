module;

#include <cstddef>
#include <cstdint>

export module module_loader;

import module_core;
import util.core;

export namespace modulex {
    struct LoadResult {
        bool ok{false};
        util::u32 entry{0};
    };

    struct Loader {
        static LoadResult load(const ImageHeader* img) noexcept {
            (void)img;
            return {false, 0};
        }
    };
}

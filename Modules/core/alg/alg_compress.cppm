module;

#include <cstddef>

export module alg_compress;

import util.core;

export namespace alg {
    enum class CompErr : util::u8 {
        ok = 0,
        out_of_space,
        malformed
    };

    struct CompResult {
        bool ok{false};
        util::usize used{0};
        CompErr err{CompErr::ok};
    };
}

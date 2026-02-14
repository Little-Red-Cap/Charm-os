module;

#include <cstdint>

export module fs_errno;

import util.core;

export namespace fs {
    enum class Err : util::i32 {
        ok = 0,
        perm = -1,
        noent = -2,
        io = -5,
        busy = -16,
        inval = -22,
        nosys = -38,
        nomem = -12,
        notsup = -95,
        again = -11,
    };
}

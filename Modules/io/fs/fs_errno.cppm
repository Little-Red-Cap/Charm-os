module;

#include <cstdint>

export module fs_errno;

import util.core;

export namespace fs {
    enum class Err : util::i32 {
        ok = 0,
        perm = -1,
        noent = -2,
        exist = -17,
        io = -5,
        busy = -16,
        inval = -22,
        nametoolong = -36,
        nosys = -38,
        nomem = -12,
        notsup = -95,
        rofs = -30,
        timeout = -110,
        again = -11,
    };
}

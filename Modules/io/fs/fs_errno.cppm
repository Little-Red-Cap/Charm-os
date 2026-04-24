module;

#include <cstdint>

export module fs_errno;

import util.core;
import util.error;

export namespace fs {
    typedef util::Errc Errc;
}

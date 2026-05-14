module;

#include <cstdlib>

export module daplink.base.core;

import daplink.base.types;

export namespace daplink::base {
    [[noreturn]] inline void halt() noexcept {
        std::abort();
    }
}

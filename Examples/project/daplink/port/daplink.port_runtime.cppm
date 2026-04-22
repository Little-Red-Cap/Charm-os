module;

#include "daplink_port_api.hpp"

export module daplink.port_runtime;

export namespace daplink::port_runtime {
    inline void init() noexcept {
        daplink::port::runtime_init();
    }

    inline void fail_fast() noexcept {
        daplink::port::fail_fast();
    }
}

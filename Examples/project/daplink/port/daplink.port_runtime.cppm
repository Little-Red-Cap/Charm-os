module;

#include "port/daplink_port_runtime_api.hpp"

export module daplink.port_runtime;

export namespace daplink::port_runtime {
    using ::daplink::port_runtime::delay_ms;
    using ::daplink::port_runtime::fail_fast;
    using ::daplink::port_runtime::init;
    using ::daplink::port_runtime::nop;
    using ::daplink::port_runtime::system_core_clock_hz;
    using ::daplink::port_runtime::tick_ms;
}

#ifndef DAPLINK_PORT_API_HPP
#define DAPLINK_PORT_API_HPP

#include "port/stm32/daplink_port_stm32_api_bundle.hpp"

namespace daplink::port {
    using UsbLayout = daplink::port::stm32::F1ScaledUsbPmaLayoutTraits<1U>;
}

#endif

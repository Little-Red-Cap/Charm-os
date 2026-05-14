#ifndef DAPLINK_PORT_API_HPP
#define DAPLINK_PORT_API_HPP

#include "platform/stm32/daplink_platform_stm32_api_bundle.hpp"

namespace daplink::port {
    using UsbLayout = daplink::platform::stm32::F1ScaledUsbPmaLayoutTraits<1U>;
}

#endif

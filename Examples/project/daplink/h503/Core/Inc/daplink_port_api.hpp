#ifndef DAPLINK_PORT_API_HPP
#define DAPLINK_PORT_API_HPP

#include "platform/stm32/daplink_platform_stm32_api_bundle.hpp"

namespace daplink::port {
    using UsbLayout = daplink::platform::stm32::UsbPmaLayoutTraits<
        daplink::platform::stm32::UsbPmaLayout{
            0x14U,
            0x54U,
            0x94U,
            0xD4U,
            0x114U,
            0x11CU,
            0x15CU,
        }>;
}

#endif

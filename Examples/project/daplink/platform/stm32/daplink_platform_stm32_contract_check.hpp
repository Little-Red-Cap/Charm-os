#ifndef DAPLINK_PLATFORM_STM32_CONTRACT_CHECK_HPP
#define DAPLINK_PLATFORM_STM32_CONTRACT_CHECK_HPP

#include "platform/daplink_platform_contract.hpp"
#include "platform/stm32/daplink_platform_stm32_api_support.hpp"
#include "platform/stm32/daplink_platform_stm32_usb_layout_support.hpp"

namespace daplink::platform::stm32::detail {
    using DefaultUsbLayout = daplink::platform::stm32::F1ScaledUsbPmaLayoutTraits<1U>;
}

static_assert(
    daplink::platform_contract::PortPlatform<daplink::platform::stm32::Platform, daplink::platform::stm32::detail::DefaultUsbLayout>,
    "STM32 platform bundle does not satisfy the DAPLink platform contract.");

#endif

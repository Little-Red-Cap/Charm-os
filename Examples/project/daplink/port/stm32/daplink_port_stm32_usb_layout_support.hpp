#ifndef DAPLINK_PORT_STM32_USB_LAYOUT_SUPPORT_HPP
#define DAPLINK_PORT_STM32_USB_LAYOUT_SUPPORT_HPP

// Legacy compatibility wrapper. New ports should include
// platform/stm32/daplink_platform_stm32_usb_layout_support.hpp directly.
#include "platform/stm32/daplink_platform_stm32_usb_layout_support.hpp"

#ifndef DAPLINK_PORT_STM32_NAMESPACE_ALIAS_HPP
#define DAPLINK_PORT_STM32_NAMESPACE_ALIAS_HPP
namespace daplink::port::stm32 = daplink::platform::stm32;
#endif

#endif

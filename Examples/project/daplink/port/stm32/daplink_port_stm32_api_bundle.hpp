#ifndef DAPLINK_PORT_STM32_API_BUNDLE_HPP
#define DAPLINK_PORT_STM32_API_BUNDLE_HPP

// Legacy compatibility wrapper. New ports should include
// platform/stm32/daplink_platform_stm32_api_bundle.hpp directly.
#include "platform/stm32/daplink_platform_stm32_api_bundle.hpp"

#ifndef DAPLINK_PORT_STM32_NAMESPACE_ALIAS_HPP
#define DAPLINK_PORT_STM32_NAMESPACE_ALIAS_HPP
namespace daplink::port::stm32 = daplink::platform::stm32;
#endif

#endif

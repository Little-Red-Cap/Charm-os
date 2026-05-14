#ifndef DAPLINK_BACKEND_STM32_SUPPORT_HPP
#define DAPLINK_BACKEND_STM32_SUPPORT_HPP

// Legacy compatibility wrapper. New ports should include
// platform/stm32/daplink_platform_stm32_backend_support.hpp directly.
#include "platform/stm32/daplink_platform_stm32_backend_support.hpp"

#ifndef DAPLINK_BACKEND_STM32_NAMESPACE_ALIAS_HPP
#define DAPLINK_BACKEND_STM32_NAMESPACE_ALIAS_HPP
namespace daplink::backend_support::stm32 = daplink::platform::stm32::backend_support;
#endif

#endif

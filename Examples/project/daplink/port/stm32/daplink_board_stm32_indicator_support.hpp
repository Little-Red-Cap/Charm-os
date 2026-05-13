#ifndef DAPLINK_BOARD_STM32_INDICATOR_SUPPORT_HPP
#define DAPLINK_BOARD_STM32_INDICATOR_SUPPORT_HPP

// Legacy compatibility wrapper. New ports should include
// platform/stm32/daplink_platform_stm32_board_indicator_support.hpp directly.
#include "platform/stm32/daplink_platform_stm32_board_indicator_support.hpp"

#ifndef DAPLINK_BOARD_STM32_NAMESPACE_ALIAS_HPP
#define DAPLINK_BOARD_STM32_NAMESPACE_ALIAS_HPP
namespace daplink::board_support::stm32 = daplink::platform::stm32::board_support;
#endif

#endif

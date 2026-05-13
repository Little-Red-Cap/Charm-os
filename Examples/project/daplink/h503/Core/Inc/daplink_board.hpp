#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "port/daplink_board_support.hpp"
#include "platform/stm32/daplink_platform_stm32_board_pinmap_support.hpp"
#include "icache.h"

namespace daplink::board_target {
    struct Traits : daplink::platform::stm32::board_support::BasicTargetPinMap<> {
        static void init_board_gpio() noexcept {
            MX_ICACHE_Init();
            MX_GPIO_Init();
        }
    };

    using TargetPins = daplink::board_support::BasicTargetPins<Traits>;
    using Indicators = daplink::board_support::BasicIndicators<Traits>;
    using UsbConnect = daplink::board_support::BasicUsbConnectSwitch<Traits>;
    using Support = daplink::board_support::BasicBoardOps<TargetPins, Indicators, UsbConnect>;
}

#endif

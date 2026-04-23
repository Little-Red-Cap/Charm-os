#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#include "port/daplink_board_support.hpp"
#include "port/stm32/daplink_board_stm32_pinmap_support.hpp"
#include "icache.h"

namespace daplink::board_target {
    struct Traits : daplink::board_support::stm32::BasicTargetPinMap<> {
        static void init_board_gpio() noexcept {
            MX_ICACHE_Init();
            MX_GPIO_Init();
        }
    };

    using Support = daplink::board_support::BasicBoardOps<Traits>;
}

#endif

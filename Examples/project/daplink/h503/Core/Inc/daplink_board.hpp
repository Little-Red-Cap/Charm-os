#ifndef DAPLINK_BOARD_HPP
#define DAPLINK_BOARD_HPP

#ifndef CHARM_STM32H5_SUPPRESS_MATH_HEADER
#define CHARM_STM32H5_SUPPRESS_MATH_HEADER 1
#define DAPLINK_STM32H5_SUPPRESS_MATH_HEADER_LOCAL 1
#endif

#include "port/daplink_board_support.hpp"
#include "icache.h"

#ifdef DAPLINK_STM32H5_SUPPRESS_MATH_HEADER_LOCAL
#undef DAPLINK_STM32H5_SUPPRESS_MATH_HEADER_LOCAL
#undef CHARM_STM32H5_SUPPRESS_MATH_HEADER
#endif

namespace daplink::board_target {
    struct Traits : daplink::board_support::DefaultTraits {
        static void init_board_gpio() noexcept {
            MX_ICACHE_Init();
            MX_GPIO_Init();
        }

        static inline daplink::port::GpioPort* const kSwclkPort = T_CLK_GPIO_Port;
        static constexpr std::uint32_t kSwclkPin = T_CLK_Pin;
        static inline daplink::port::GpioPort* const kSwdioInPort = T_DIO_IN_GPIO_Port;
        static constexpr std::uint32_t kSwdioInPin = T_DIO_IN_Pin;
        static inline daplink::port::GpioPort* const kSwdioOutPort = T_DIO_OUT_GPIO_Port;
        static constexpr std::uint32_t kSwdioOutPin = T_DIO_OUT_Pin;
        static inline daplink::port::GpioPort* const kResetPort = T_RST_GPIO_Port;
        static constexpr std::uint32_t kResetPin = T_RST_Pin;
    };

    using Support = daplink::board_support::BasicBoardOps<Traits>;
}

#endif

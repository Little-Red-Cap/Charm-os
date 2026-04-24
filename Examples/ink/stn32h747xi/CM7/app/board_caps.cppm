module;

export module platform.board.stn32h747xi;

import platform.board;
import hal_core;
import hal_uart;
import util.core;

extern "C" {
    void* charm_uart1_ctx(void) noexcept;
    int charm_uart_init(void* ctx, util::u32 baud, util::u8 data_bits,
                        util::u8 parity, util::u8 stop_bits) noexcept;
    int charm_uart_enable(void* ctx) noexcept;
    int charm_uart_disable(void* ctx) noexcept;
    int charm_uart_try_write(void* ctx, util::u8 byte) noexcept;
    int charm_uart_try_read(void* ctx, util::u8* byte) noexcept;
    void charm_uart_enable_irq(void* ctx, util::u32 mask) noexcept;
    void charm_uart_disable_irq(void* ctx, util::u32 mask) noexcept;
    void charm_uart_clear_irq(void* ctx, util::u32 mask) noexcept;
    util::u32 HAL_GetTick(void);
}

namespace platform::board::stn32h747xi::detail {
    constexpr int kHalOk = 0;
    constexpr int kHalError = 1;
    constexpr int kHalBusy = 2;
    constexpr int kHalTimeout = 3;
    constexpr int kHalUnsupported = 4;

    hal::Result map_result(int code) noexcept {
        switch (code) {
        case kHalOk:
            return hal::ok();
        case kHalBusy:
            return hal::err(hal::Status::busy);
        case kHalTimeout:
            return hal::err(hal::Status::timeout);
        case kHalUnsupported:
            return hal::err(hal::Status::unsupported);
        case kHalError:
        default:
            return hal::err(hal::Status::error);
        }
    }

    hal::Result stm32_uart_init(void* ctx, const hal::UartConfig& cfg) noexcept {
        const auto code = charm_uart_init(ctx, cfg.baud, cfg.data_bits,
                                          static_cast<util::u8>(cfg.parity),
                                          static_cast<util::u8>(cfg.stop_bits));
        return map_result(code);
    }

    hal::Result stm32_uart_enable(void* ctx) noexcept {
        return map_result(charm_uart_enable(ctx));
    }

    hal::Result stm32_uart_disable(void* ctx) noexcept {
        return map_result(charm_uart_disable(ctx));
    }

    hal::Result stm32_uart_try_write(void* ctx, util::u8 byte) noexcept {
        return map_result(charm_uart_try_write(ctx, byte));
    }

    hal::Result stm32_uart_try_read(void* ctx, util::u8& byte) noexcept {
        const auto code = charm_uart_try_read(ctx, &byte);
        return map_result(code);
    }

    void stm32_uart_enable_irq(void* ctx, util::u32 mask) noexcept {
        charm_uart_enable_irq(ctx, mask);
    }

    void stm32_uart_disable_irq(void* ctx, util::u32 mask) noexcept {
        charm_uart_disable_irq(ctx, mask);
    }

    void stm32_uart_clear_irq(void* ctx, util::u32 mask) noexcept {
        charm_uart_clear_irq(ctx, mask);
    }

    util::u64 stm32_now_ms(void*) noexcept {
        return static_cast<util::u64>(HAL_GetTick());
    }
} // namespace platform::board::stn32h747xi::detail

export namespace platform::board::stn32h747xi {
    inline ConsoleCaps make_console_caps() noexcept {
        void* uart1_ctx = charm_uart1_ctx();
        static const hal::UartOps kStm32UartOps{
            &detail::stm32_uart_init,
            &detail::stm32_uart_enable,
            &detail::stm32_uart_disable,
            &detail::stm32_uart_try_write,
            &detail::stm32_uart_try_read,
            &detail::stm32_uart_enable_irq,
            &detail::stm32_uart_disable_irq,
            &detail::stm32_uart_clear_irq
        };
        ConsoleCaps caps{};
        caps.uart.handle = hal::UartIoHandle{uart1_ctx, &kStm32UartOps};
        caps.uart.config = hal::UartConfig{};
        caps.uart.io_cap = "io.uart1";
        caps.uart.hal_cap = "hal.uart1";
        caps.clock = ClockDesc{nullptr, &detail::stm32_now_ms, nullptr};
        caps.console_cap = "io.console0";
        return caps;
    }

    inline InputCaps make_input_caps() noexcept {
        InputCaps caps{};
        caps.clock = ClockDesc{nullptr, &detail::stm32_now_ms, nullptr};
        return caps;
    }

    inline BlockCaps make_block_caps() noexcept {
        BlockCaps caps{};
        caps.clock = ClockDesc{nullptr, &detail::stm32_now_ms, nullptr};
        return caps;
    }

    inline BoardCaps make_board_caps() noexcept {
        void* uart1_ctx = charm_uart1_ctx();
        static const hal::UartOps kStm32UartOps{
            &detail::stm32_uart_init,
            &detail::stm32_uart_enable,
            &detail::stm32_uart_disable,
            &detail::stm32_uart_try_write,
            &detail::stm32_uart_try_read,
            &detail::stm32_uart_enable_irq,
            &detail::stm32_uart_disable_irq,
            &detail::stm32_uart_clear_irq
        };
        BoardCaps caps{};
        caps.uart1.handle = hal::UartIoHandle{uart1_ctx, &kStm32UartOps};
        caps.uart1.config = hal::UartConfig{};
        caps.uart1.io_cap = "io.uart1";
        caps.uart1.hal_cap = "hal.uart1";
        caps.clock = ClockDesc{nullptr, &detail::stm32_now_ms, nullptr};
        caps.console_cap = "io.console0";
        return caps;
    }
}

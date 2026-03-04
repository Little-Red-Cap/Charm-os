module;

export module platform.board.win_stub;

import platform.board;
import platform.win.time_source;
import hal_core;
import hal_uart;
import hal_win;
import util.core;

namespace platform::board::win_stub::detail {
    struct WinUartCtx {
        hal::UartHandle handle{};
    };

    hal::Result win_uart_init(void* ctx, const hal::UartConfig& cfg) noexcept {
        auto* self = static_cast<WinUartCtx*>(ctx);
        return hal::win::Uart::init(self->handle, cfg);
    }

    hal::Result win_uart_enable(void* ctx) noexcept {
        auto* self = static_cast<WinUartCtx*>(ctx);
        return hal::win::Uart::enable(self->handle);
    }

    hal::Result win_uart_disable(void* ctx) noexcept {
        auto* self = static_cast<WinUartCtx*>(ctx);
        return hal::win::Uart::disable(self->handle);
    }

    hal::Result win_uart_try_write(void* ctx, util::u8 byte) noexcept {
        auto* self = static_cast<WinUartCtx*>(ctx);
        return hal::win::Uart::try_write(self->handle, byte);
    }

    hal::Result win_uart_try_read(void* ctx, util::u8& byte) noexcept {
        auto* self = static_cast<WinUartCtx*>(ctx);
        return hal::win::Uart::try_read(self->handle, byte);
    }

    void win_uart_enable_irq(void* ctx, util::u32 mask) noexcept {
        auto* self = static_cast<WinUartCtx*>(ctx);
        hal::win::Uart::enable_irq(self->handle, mask);
    }

    void win_uart_disable_irq(void* ctx, util::u32 mask) noexcept {
        auto* self = static_cast<WinUartCtx*>(ctx);
        hal::win::Uart::disable_irq(self->handle, mask);
    }

    void win_uart_clear_irq(void* ctx, util::u32 mask) noexcept {
        auto* self = static_cast<WinUartCtx*>(ctx);
        hal::win::Uart::clear_irq(self->handle, mask);
    }

    util::u64 win_now_us(void*) noexcept {
        return platform::win::SteadyClock::now();
    }
} // namespace platform::board::win_stub::detail

export namespace platform::board::win_stub {
    inline BoardCaps make_board_caps() noexcept {
        static detail::WinUartCtx uart1_ctx{hal::UartHandle{1, nullptr}};
        static const hal::UartOps kWinUartOps{
            &detail::win_uart_init,
            &detail::win_uart_enable,
            &detail::win_uart_disable,
            &detail::win_uart_try_write,
            &detail::win_uart_try_read,
            &detail::win_uart_enable_irq,
            &detail::win_uart_disable_irq,
            &detail::win_uart_clear_irq
        };
        BoardCaps caps{};
        caps.uart1.handle = hal::UartIoHandle{&uart1_ctx, &kWinUartOps};
        caps.uart1.config = hal::UartConfig{};
        caps.uart1.io_cap = "io.uart1";
        caps.uart1.hal_cap = "hal.uart1";
        caps.clock = ClockDesc{nullptr, nullptr, &detail::win_now_us};
        return caps;
    }
}

module;

export module platform.board.win_stub;

import platform.board;
import hal_core;
import hal_uart;
import hal_win;
import util.core;

namespace {
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

} // namespace

export namespace platform::board::win_stub {
    inline BoardCaps make_board_caps() noexcept {
        static WinUartCtx uart1_ctx{hal::UartHandle{1, nullptr}};
        static const hal::UartOps kWinUartOps{
            &win_uart_init,
            &win_uart_enable,
            &win_uart_disable,
            &win_uart_try_write,
            &win_uart_try_read,
            &win_uart_enable_irq,
            &win_uart_disable_irq,
            &win_uart_clear_irq
        };
        BoardCaps caps{};
        caps.uart1.handle = hal::UartIoHandle{&uart1_ctx, &kWinUartOps};
        caps.uart1.config = hal::UartConfig{};
        caps.uart1.io_cap = "io.uart1";
        caps.uart1.hal_cap = "hal.uart1";
        return caps;
    }
}

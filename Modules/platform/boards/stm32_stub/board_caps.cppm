module;

export module platform.board.stm32_stub;

import platform.board;
import hal_core;
import hal_uart;
import hal_stm32_stub;

namespace {
    struct Stm32UartCtx {
        hal::UartHandle handle{};
    };

    hal::Result stm32_uart_init(void* ctx, const hal::UartConfig& cfg) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::init(self->handle, cfg);
    }

    hal::Result stm32_uart_enable(void* ctx) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::enable(self->handle);
    }

    hal::Result stm32_uart_disable(void* ctx) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::disable(self->handle);
    }

    hal::Result stm32_uart_try_write(void* ctx, util::u8 byte) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::try_write(self->handle, byte);
    }

    hal::Result stm32_uart_try_read(void* ctx, util::u8& byte) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        return hal::stm32_stub::Uart::try_read(self->handle, byte);
    }

    void stm32_uart_enable_irq(void* ctx, util::u32 mask) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        hal::stm32_stub::Uart::enable_irq(self->handle, mask);
    }

    void stm32_uart_disable_irq(void* ctx, util::u32 mask) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        hal::stm32_stub::Uart::disable_irq(self->handle, mask);
    }

    void stm32_uart_clear_irq(void* ctx, util::u32 mask) noexcept {
        auto* self = static_cast<Stm32UartCtx*>(ctx);
        hal::stm32_stub::Uart::clear_irq(self->handle, mask);
    }
} // namespace

export namespace platform::board::stm32_stub {
    inline BoardCaps make_board_caps() noexcept {
        static Stm32UartCtx uart1_ctx{hal::UartHandle{1, nullptr}};
        static const hal::UartOps kStm32UartOps{
            &stm32_uart_init,
            &stm32_uart_enable,
            &stm32_uart_disable,
            &stm32_uart_try_write,
            &stm32_uart_try_read,
            &stm32_uart_enable_irq,
            &stm32_uart_disable_irq,
            &stm32_uart_clear_irq
        };
        BoardCaps caps{};
        caps.uart1.handle = hal::UartIoHandle{&uart1_ctx, &kStm32UartOps};
        caps.uart1.config = hal::UartConfig{};
        caps.uart1.io_cap = "io.uart1";
        caps.uart1.hal_cap = "hal.uart1";
        return caps;
    }
}

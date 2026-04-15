module;

#include <array>

export module platform.board.win_stub;

import platform.board;
import platform.win.time_source;
import hal_core;
import hal_input;
import hal_uart;
import hal_win;
import io.channel;
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

    util::u64 win_now_ms(void* ctx) noexcept {
        return win_now_us(ctx) / 1000u;
    }

    struct ByteRing {
        std::array<util::u8, 512> buf{};
        util::usize head{0};
        util::usize tail{0};
        util::usize count{0};

        util::usize push(io::ByteView in) noexcept {
            util::usize n = 0;
            while (n < in.size() && count < buf.size()) {
                buf[tail] = in[n++];
                tail = (tail + 1) % buf.size();
                ++count;
            }
            return n;
        }

        util::usize pop(io::MutByteView out) noexcept {
            util::usize n = 0;
            while (n < out.size() && count > 0) {
                out[n++] = buf[head];
                head = (head + 1) % buf.size();
                --count;
            }
            return n;
        }
    };

    struct CanLoopbackCtx {
        ByteRing ring{};
    };

    io::result win_can_read(void* ctx, io::MutByteView buf) noexcept {
        if (!ctx || buf.empty()) return io::fail(io::errc::invalid_arg);
        auto* self = static_cast<CanLoopbackCtx*>(ctx);
        const auto n = self->ring.pop(buf);
        if (n == 0) return io::fail(io::errc::would_block);
        return io::ok(n);
    }

    io::result win_can_write(void* ctx, io::ByteView buf) noexcept {
        if (!ctx || buf.empty()) return io::fail(io::errc::invalid_arg);
        auto* self = static_cast<CanLoopbackCtx*>(ctx);
        const auto n = self->ring.push(buf);
        if (n == 0) return io::fail(io::errc::would_block);
        return io::ok(n);
    }

    io::result win_can_flush(void* ctx) noexcept {
        if (!ctx) return io::fail(io::errc::invalid_arg);
        return io::ok(0);
    }
} // namespace platform::board::win_stub::detail

export namespace platform::board::win_stub {
    inline BootBoardCaps make_boot_caps() noexcept {
        return {};
    }

    inline ConsoleCaps make_console_caps() noexcept {
        static detail::WinUartCtx uart1_ctx{hal::UartHandle{1, nullptr}};
        static const hal::UartOps kWinUartOps{
            &detail::win_uart_init,
            &detail::win_uart_enable,
            &detail::win_uart_disable,
            &detail::win_uart_try_write,
            &detail::win_uart_try_read,
            nullptr,
            nullptr,
            nullptr
        };
        ConsoleCaps caps{};
        caps.uart.handle = hal::UartIoHandle{&uart1_ctx, &kWinUartOps};
        caps.uart.config = hal::UartConfig{};
        caps.uart.io_cap = "io.uart1";
        caps.uart.hal_cap = "hal.uart1";
        caps.clock = ClockDesc{nullptr, &detail::win_now_ms, &detail::win_now_us};
        caps.console_cap = "io.console0";
        return caps;
    }

    inline InputCaps make_input_caps() noexcept {
        static const hal::RawInputDriver kWinRawInput = hal::win::RawInput::driver();
        InputCaps caps{};
        caps.input.driver = &kWinRawInput;
        caps.clock = ClockDesc{nullptr, &detail::win_now_ms, &detail::win_now_us};
        return caps;
    }

    inline BlockCaps make_block_caps() noexcept {
        BlockCaps caps{};
        caps.clock = ClockDesc{nullptr, &detail::win_now_ms, &detail::win_now_us};
        return caps;
    }

    inline BoardCaps make_board_caps() noexcept {
        static detail::WinUartCtx uart1_ctx{hal::UartHandle{1, nullptr}};
        static const hal::UartOps kWinUartOps{
            &detail::win_uart_init,
            &detail::win_uart_enable,
            &detail::win_uart_disable,
            &detail::win_uart_try_write,
            &detail::win_uart_try_read,
            nullptr,
            nullptr,
            nullptr
        };
        static const hal::RawInputDriver kWinRawInput = hal::win::RawInput::driver();
        static detail::CanLoopbackCtx can0_ctx{};
        static io::Channel can0_channel{
            &can0_ctx,
            io::ChannelOps{&detail::win_can_read, &detail::win_can_write, &detail::win_can_flush}
        };
        BoardCaps caps{};
        caps.uart1.handle = hal::UartIoHandle{&uart1_ctx, &kWinUartOps};
        caps.uart1.config = hal::UartConfig{};
        caps.uart1.io_cap = "io.uart1";
        caps.uart1.hal_cap = "hal.uart1";
        caps.clock = ClockDesc{nullptr, &detail::win_now_ms, &detail::win_now_us};
        caps.console_cap = "io.console0";
        caps.input.driver = &kWinRawInput;
        caps.can0.channel = &can0_channel;
        caps.can0.io_cap = "io.can0";
        return with_boot_caps(caps, make_boot_caps());
    }
}
